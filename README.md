# <img src="assets/muhpark.png" width="30" height="30" /> MuhPark

Smart parking sensor — ESP32-S2 Mini + VL53L1X + MAX7219 8×8 LED matrix.

## Hardware

| Component | Part |
|---|---|
| MCU | LOLIN S2 Mini (ESP32-S2) |
| Sensor | VL53L1X time-of-flight (I2C) |
| Display | 8×8 LED matrix, MAX7219 driver |

## Wiring

### VL53L1X

```
VL53L1X VIN  →  ESP32 3.3 V
VL53L1X GND  →  ESP32 GND
VL53L1X SDA  →  GPIO 33
VL53L1X SCL  →  GPIO 35
```

No voltage divider needed — the sensor runs at 3.3 V natively.

### MAX7219

```
MAX7219 VCC  →  ESP32 5V
MAX7219 GND  →  ESP32 GND
MAX7219 DIN  →  GPIO 7
MAX7219 CLK  →  GPIO 3
MAX7219 CS   →  GPIO 5
```

> **Note:** MAX7219 logic threshold is ~3.5 V. Driving directly from 3.3 V GPIO
> usually works; for guaranteed reliability add a 74AHCT125 level shifter on
> DIN and CLK.

## Configuration

Edit `platformio.ini` and set your credentials in `build_flags`:

```ini
build_flags =
    -DWIFI_SSID=\"MyNetwork\"
    -DWIFI_PASSWORD=\"MyPassword\"
```

If WiFi fails the device starts an AP named **muhpark-ap** (no password).
Connect and browse to `192.168.4.1`.

## Build & Flash

```bash
# Install PlatformIO CLI (skip if already installed)
pip install platformio

# Flash firmware
pio run -t upload

# Flash web UI (LittleFS image)
pio run -t uploadfs

# Monitor serial
pio device monitor
```

Both `upload` and `uploadfs` are required on first flash. After that, OTA
updates (`ArduinoOTA`) can replace the firmware without USB.

## Display Logic

| Distance | Level shown |
|---|---|
| 200–300 cm | **5** |
| 150–200 cm | **4** |
| 100–150 cm | **3** |
| 50–100 cm  | **2** |
| 25–50 cm   | **1** |
| ≤ 25 cm    | smiley (parked) |
| > 300 cm   | off |

Each boundary has ~10% hysteresis to prevent flickering.

## State Machine

```
             object detected (≤ 300 cm)
  SLEEPING ─────────────────────────────► ACTIVE
     ▲                                      │  ▲
     │  60 s timeout                        │  │ object in range
     │                                      ▼  │
   IDLE ◄─────────────────────────────── object leaves (> 330 cm)
```

LED matrix is on only during **ACTIVE**. Both IDLE and SLEEPING keep it off.

## Web Interface

Open `http://<device-ip>` in any browser.

- Live distance and parking level over WebSocket
- State badge (ACTIVE / IDLE / SLEEPING)
- Progress bar for remaining distance
- Automatic reconnect with exponential back-off

REST fallback: `GET /api/status` returns JSON (useful for scripting).

## Architecture

```
loop()
  every 100 ms (sensor dataReady):
    readRaw()            — VL53L1X single ToF measurement (mm → cm)
    medianFilter()       — 7-sample sliding window
    updateLevel()        — hysteresis state machine (levels 0–6)
    updateStateMachine() — SLEEPING / IDLE / ACTIVE + LED output

  every 200 ms:
    broadcastStatus()    — JSON over WebSocket to all clients
```

## Adjusting Parameters

All key constants are at the top of `src/sensor.cpp`:

| Constant | Default | Purpose |
|---|---|---|
| `INNER_CLOSER[]` | see code | per-level activation thresholds (cm) |
| `INNER_FARTHER[]` | see code | per-level deactivation thresholds (cm) |
| `LEVEL_DEBOUNCE` | 5 | readings before a level change is committed |
| `FILTER_SIZE` | 7 | median filter window |
