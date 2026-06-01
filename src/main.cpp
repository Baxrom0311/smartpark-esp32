// SmartPark ESP32 firmware
// ─────────────────────────────────────────────────────────────────────────
// Reads 4 HC-SR04 ultrasonic sensors every SMARTPARK_READ_INTERVAL_MS,
// classifies each slot as occupied/free using SMARTPARK_OCCUPIED_CM,
// drives a status LED per slot, and POSTs the readings to the FastAPI
// backend. To minimise network noise we only POST when at least one
// slot's occupancy bit changed since the last successful upload, plus a
// periodic heartbeat (SMARTPARK_HEARTBEAT_MS) so the server can recover
// state after a reboot.
//
// Backend contract (PROJECT_BRIEF.md → POST /api/device/sensor-data):
//
//     headers:  X-Device-Key: <SMARTPARK_DEVICE_KEY>
//     body:
//       {
//         "device_id": "esp32_001",
//         "sensors": [
//           {"slot_number": 1, "distance_cm": 5.2,   "is_occupied": true},
//           {"slot_number": 2, "distance_cm": 150.0, "is_occupied": false}
//         ]
//       }

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"

namespace {

constexpr float    kSpeedOfSoundCmPerUs = 0.0343f / 2.0f;  // round-trip
constexpr uint32_t kWifiConnectTimeoutMs = 20000UL;
constexpr uint32_t kWifiRetryBackoffMs   = 5000UL;
constexpr uint32_t kHttpTimeoutMs        = 5000UL;

struct SlotState {
    int   slot_number;
    int   trig_pin;
    int   echo_pin;
    int   led_pin;
    float last_distance_cm;
    bool  last_occupied;
    bool  has_reading;
};

SlotState g_slots[SMARTPARK_SLOT_COUNT];

uint32_t g_last_read_at      = 0;
uint32_t g_last_post_at      = 0;
bool     g_dirty_since_post  = true;   // force first publish after boot

// ─── WiFi ──────────────────────────────────────────────────────────────
void connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.printf("[wifi] connecting to \"%s\" ...\n", SMARTPARK_WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SMARTPARK_WIFI_SSID, SMARTPARK_WIFI_PASSWORD);

    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - started < kWifiConnectTimeoutMs) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] online, ip=%s rssi=%d dBm\n",
                      WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
    } else {
        Serial.printf("[wifi] failed, will retry in %lums\n",
                      kWifiRetryBackoffMs);
    }
}

// ─── HC-SR04 ──────────────────────────────────────────────────────────
// Returns NaN if no echo received before timeout.
float readDistanceCm(const SlotState& s) {
    digitalWrite(s.trig_pin, LOW);
    delayMicroseconds(2);
    digitalWrite(s.trig_pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(s.trig_pin, LOW);

    const uint32_t pulse_us =
        pulseIn(s.echo_pin, HIGH, SMARTPARK_ECHO_TIMEOUT_US);

    if (pulse_us == 0) {
        return NAN;  // timeout → no object within range
    }
    return static_cast<float>(pulse_us) * kSpeedOfSoundCmPerUs;
}

void initSlots() {
    for (size_t i = 0; i < SMARTPARK_SLOT_COUNT; ++i) {
        const SlotPins& cfg = SMARTPARK_SLOTS[i];
        g_slots[i] = SlotState{
            cfg.slot_number,
            cfg.trig_pin,
            cfg.echo_pin,
            cfg.led_pin,
            0.0f,
            false,
            false,
        };
        pinMode(cfg.trig_pin, OUTPUT);
        digitalWrite(cfg.trig_pin, LOW);
        pinMode(cfg.echo_pin, INPUT);
        if (cfg.led_pin >= 0) {
            pinMode(cfg.led_pin, OUTPUT);
            digitalWrite(cfg.led_pin, LOW);
        }
    }
}

// Sample every sensor; mark g_dirty_since_post if any slot's occupancy
// flipped vs. its last reported value.
void sampleSlots() {
    for (size_t i = 0; i < SMARTPARK_SLOT_COUNT; ++i) {
        SlotState& s = g_slots[i];
        const float distance = readDistanceCm(s);

        // No echo → treat as out-of-range (free). Reported distance is
        // a sentinel large value so the backend log stays meaningful.
        const bool reading_valid = !isnan(distance);
        const float reported_cm  = reading_valid ? distance : 999.0f;
        const bool occupied =
            reading_valid && reported_cm < SMARTPARK_OCCUPIED_CM;

        if (!s.has_reading || occupied != s.last_occupied) {
            g_dirty_since_post = true;
        }
        s.last_distance_cm = reported_cm;
        s.last_occupied    = occupied;
        s.has_reading      = true;

        if (s.led_pin >= 0) {
            digitalWrite(s.led_pin, occupied ? HIGH : LOW);
        }

        Serial.printf("[slot %d] %.1f cm → %s\n",
                      s.slot_number,
                      reported_cm,
                      occupied ? "OCCUPIED" : "free");
    }
}

// ─── HTTP ─────────────────────────────────────────────────────────────
String buildPayload() {
    JsonDocument doc;
    doc["device_id"] = SMARTPARK_DEVICE_ID;
    JsonArray sensors = doc["sensors"].to<JsonArray>();
    for (size_t i = 0; i < SMARTPARK_SLOT_COUNT; ++i) {
        const SlotState& s = g_slots[i];
        JsonObject item = sensors.add<JsonObject>();
        item["slot_number"] = s.slot_number;
        // round to 1 decimal — backend column is float, no need for noise
        item["distance_cm"] = roundf(s.last_distance_cm * 10.0f) / 10.0f;
        item["is_occupied"] = s.last_occupied;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

bool postSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[http] skip POST — wifi offline");
        return false;
    }

    const String payload = buildPayload();

    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(SMARTPARK_API_URL)) {
        Serial.println("[http] begin() failed (bad URL?)");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", SMARTPARK_DEVICE_KEY);

    const int status = http.POST(payload);
    bool ok = false;
    if (status > 0) {
        Serial.printf("[http] POST → %d  bytes=%u\n", status, payload.length());
        if (status >= 200 && status < 300) {
            ok = true;
        } else {
            const String body = http.getString();
            Serial.printf("[http] error body: %s\n", body.c_str());
        }
    } else {
        Serial.printf("[http] transport error: %s\n",
                      HTTPClient::errorToString(status).c_str());
    }
    http.end();
    return ok;
}

bool shouldPublish(uint32_t now) {
    if (g_dirty_since_post) return true;
    if (SMARTPARK_HEARTBEAT_MS == 0) return false;
    return (now - g_last_post_at) >= SMARTPARK_HEARTBEAT_MS;
}

}  // namespace

// ─── Arduino entrypoints ───────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("==== SmartPark ESP32 firmware ====");
    Serial.printf("device_id=%s slots=%u threshold=%.1fcm\n",
                  SMARTPARK_DEVICE_ID,
                  static_cast<unsigned>(SMARTPARK_SLOT_COUNT),
                  SMARTPARK_OCCUPIED_CM);

    initSlots();
    connectWifi();
}

void loop() {
    const uint32_t now = millis();

    // millis() wraps every ~49 days — using unsigned subtraction handles
    // the rollover safely.
    if (now - g_last_read_at >= SMARTPARK_READ_INTERVAL_MS) {
        g_last_read_at = now;

        if (WiFi.status() != WL_CONNECTED) {
            connectWifi();
        }

        sampleSlots();

        if (shouldPublish(now)) {
            if (postSensorData()) {
                g_last_post_at     = now;
                g_dirty_since_post = false;
            }
        }
    }

    // Yield to WiFi/TCP stack.
    delay(10);
}
