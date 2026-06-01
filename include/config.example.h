// SmartPark ESP32 — config template
//
// Copy this file to `include/config.h` and fill in your values.
// `config.h` is gitignored so secrets never leave your machine.
//
//     cp include/config.example.h include/config.h
//     # edit include/config.h

#pragma once

// ─── WiFi ───────────────────────────────────────────────────────────────
#define SMARTPARK_WIFI_SSID      "YOUR_WIFI_SSID"
#define SMARTPARK_WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

// ─── Backend ────────────────────────────────────────────────────────────
// Full URL of the FastAPI ingestion endpoint.
// Use HTTP for LAN testing; HTTPS for production.
#define SMARTPARK_API_URL        "http://192.168.1.100:8000/api/device/sensor-data"

// Must match backend `DEVICE_API_KEY` env var.
#define SMARTPARK_DEVICE_KEY     "change-me-device-secret"

// Stable identifier for this physical board (max 50 chars).
#define SMARTPARK_DEVICE_ID      "esp32_001"

// ─── Sensor / control loop ─────────────────────────────────────────────
// Distance threshold (cm). Below this → slot considered occupied.
#define SMARTPARK_OCCUPIED_CM       20.0f

// How often to read sensors (ms).
#define SMARTPARK_READ_INTERVAL_MS  2000UL

// Heartbeat: even with no change, push current state every N ms so the
// backend stays in sync after restarts. Set to 0 to disable.
#define SMARTPARK_HEARTBEAT_MS      30000UL

// HC-SR04 echo timeout (us). 30 ms ≈ 5 m max range — anything farther
// is treated as "no echo" and reported as free.
#define SMARTPARK_ECHO_TIMEOUT_US   30000UL

// ─── GPIO pin map ──────────────────────────────────────────────────────
// 4 × HC-SR04: trigger + echo. Indicator LEDs are optional;
// set a pin to -1 to disable that LED.
//
// Slot N occupied → LED N solid HIGH; free → LOW.
struct SlotPins {
    int slot_number;   // 1-based, sent to backend
    int trig_pin;      // HC-SR04 TRIG
    int echo_pin;      // HC-SR04 ECHO (use voltage divider: 3.3V tolerant)
    int led_pin;       // status LED, -1 if not wired
};

static const SlotPins SMARTPARK_SLOTS[] = {
    { 1,  5, 18,  2 },   // slot 1 — onboard LED on GPIO2
    { 2, 19, 21, 14 },   // slot 2
    { 3, 22, 23, 12 },   // slot 3
    { 4, 25, 26, 13 },   // slot 4
};
static const size_t SMARTPARK_SLOT_COUNT =
    sizeof(SMARTPARK_SLOTS) / sizeof(SMARTPARK_SLOTS[0]);
