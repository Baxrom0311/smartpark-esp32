// SmartPark — LilyGo T-Beam v1.1 config template
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
#define SMARTPARK_DEVICE_KEY     "esp32-device-secret-key"

// Stable identifier for this physical board (max 50 chars).
#define SMARTPARK_DEVICE_ID      "tbeam_001"

// ─── Sensor / control loop ─────────────────────────────────────────────
// Distance threshold (cm). Below this → slot considered occupied.
#define SMARTPARK_OCCUPIED_CM       20.0f

// How often to read sensors (ms).
#define SMARTPARK_READ_INTERVAL_MS  2000UL

// Heartbeat: even with no change, push current state every N ms so the
// backend stays in sync after restarts. Set to 0 to disable.
#define SMARTPARK_HEARTBEAT_MS      30000UL

// HC-SR04 echo timeout (us). 30 ms ≈ 5 m max range.
#define SMARTPARK_ECHO_TIMEOUT_US   30000UL

// ─── GPIO pin map — LilyGo T-Beam v1.1 ────────────────────────────────
//
// T-Beam v1.1 reserved pins (do NOT use):
//   LoRa SX1276 : SCK=5  MISO=19  MOSI=27  SS=18  RST=23  DIO0=26
//   AXP192 I2C  : SDA=21 SCL=22
//   GPS UART1   : TX=12  RX=34
//   Boot button : 38
//
// Safe free pins for HC-SR04:
//   Trigger (OUTPUT): 4, 2, 13, 14, 15, 25, 32, 33
//   Echo    (INPUT) : 35, 32, 33, 25  (GPIO35 is input-only — ideal for echo)
//
// Note: GPIO35 has no internal pull-up/pull-down — fine for HC-SR04 echo.
// Note: GPIO2 is a strapping pin; keep LOW at boot (it is LOW when nothing
//       is connected, so HC-SR04 trig on GPIO2 is safe in practice).
//
// set led_pin = -1 to disable LED for that slot (T-Beam has no per-slot LEDs).

struct SlotPins {
    int slot_number;   // 1-based, sent to backend
    int trig_pin;      // HC-SR04 TRIG (OUTPUT)
    int echo_pin;      // HC-SR04 ECHO (INPUT, 3.3 V tolerant — use voltage divider if needed)
    int led_pin;       // status LED, -1 if not wired
};

static const SlotPins SMARTPARK_SLOTS[] = {
    { 1,  4, 35, -1 },   // slot 1 — trig=GPIO4  echo=GPIO35
    { 2, 13, 32, -1 },   // slot 2 — trig=GPIO13 echo=GPIO32
    { 3,  2, 33, -1 },   // slot 3 — trig=GPIO2  echo=GPIO33
    { 4, 23, 25, -1 },   // slot 4 — trig=GPIO23 echo=GPIO25
};
static const size_t SMARTPARK_SLOT_COUNT =
    sizeof(SMARTPARK_SLOTS) / sizeof(SMARTPARK_SLOTS[0]);
