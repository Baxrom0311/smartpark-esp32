# SmartPark ESP32 firmware

ESP32 DevKit + 4 × HC-SR04 ultrasonik sensorlar yordamida parkovka
slotlarining holatini o'qib, FastAPI backendga real-time yuboruvchi
firmware.

## Tarkibi

```
esp32/
├── platformio.ini          # PlatformIO env (espressif32@6.5.0, esp32dev)
├── include/
│   ├── config.example.h    # shablon — bu git-ga commit qilinadi
│   └── config.h            # haqiqiy sozlamalar (gitignored)
└── src/
    └── main.cpp            # firmware logikasi
```

## Ishga tushirish

1. PlatformIO o'rnatilganligini tekshiring:
   ```bash
   pio --version
   ```
2. Sozlamalar faylini yarating va to'ldiring:
   ```bash
   cp include/config.example.h include/config.h
   $EDITOR include/config.h
   ```
   Kamida `SMARTPARK_WIFI_SSID`, `SMARTPARK_WIFI_PASSWORD`,
   `SMARTPARK_API_URL`, `SMARTPARK_DEVICE_KEY` qiymatlarini o'rnating.
   `SMARTPARK_DEVICE_KEY` backenddagi `DEVICE_API_KEY` env bilan
   bir xil bo'lishi shart.
3. Build:
   ```bash
   pio run
   ```
4. Yuklash (ESP32 USB-ga ulanganda):
   ```bash
   pio run -t upload
   pio device monitor
   ```

## Pinout (default)

| Slot | TRIG | ECHO | LED |
| ---- | ---- | ---- | --- |
| 1    | 5    | 18   | 2   |
| 2    | 19   | 21   | 14  |
| 3    | 22   | 23   | 12  |
| 4    | 25   | 26   | 13  |

> HC-SR04 5 V chiqishi 3.3 V ESP32 ECHO pinini buzishi mumkin —
> divider rezistor (1 kΩ + 2 kΩ) qo'ying.

## Logika

- Har `SMARTPARK_READ_INTERVAL_MS` (default 2 s) sensorlardan o'qiydi.
- `distance < SMARTPARK_OCCUPIED_CM` (default 20 sm) → `is_occupied=true`.
- Faqat **kamida bitta slot holati o'zgarganda** backend-ga POST yuboradi
  (network noise minimal). Qo'shimcha ravishda `SMARTPARK_HEARTBEAT_MS`
  (default 30 s) bo'yicha holatni qayta yuborib, backend reboot dan
  keyin sinxronlashishini ta'minlaydi.
- POST so'rovi `X-Device-Key` headeri orqali autentifikatsiya qiladi.
- Har slot uchun LED: band → `HIGH`, bo'sh → `LOW`.
- WiFi uzilsa avtomatik qayta ulanadi (har siklda tekshiradi).

## API contract

`POST <SMARTPARK_API_URL>`

```json
{
  "device_id": "esp32_001",
  "sensors": [
    {"slot_number": 1, "distance_cm": 5.2,   "is_occupied": true},
    {"slot_number": 2, "distance_cm": 150.0, "is_occupied": false}
  ]
}
```

Headers: `Content-Type: application/json`, `X-Device-Key: <secret>`.
