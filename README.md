# ESP32-S3 Inverter Monitor Dashboard

**240×240 ST7789 Multi-page Auto-rotating Dashboard** for monitoring Pure Sine Wave / Hybrid Inverters.

![Dashboard Preview](docs/preview.png)

## Features

- **12 Pages** auto-rotate every 15 seconds
- Real-time **AC Voltage, Current, Power, Frequency, Power Factor**
- **Battery Voltage & SOC** (12V system linear)
- **Power Factor** calculated via zero-crossing phase detection (ZMPT101B + ACS712)
- **BMP180** Temperature / Pressure / Altitude
- **OpenWeatherMap** Weather page
- **Energy counters** (Today / Yesterday / Month / Total) stored in **NVS** (survives reboot)
- **Alarm system** with priority page jump + buzzer
- Min/Max logging
- WiFi + NTP time sync

## Hardware

| Component                    | Model / Spec              | Notes                      |
|-----------------------------|---------------------------|----------------------------|
| MCU                         | ESP32-S3 (any DevKit)     | 16MB Flash recommended     |
| Display                     | 1.54" IPS ST7789 240×240  | SPI                        |
| AC Voltage Sensor           | ZMPT101B                  |                            |
| AC Current Sensor           | ACS712 20A                |                            |
| DC Voltage Sensor           | 0-25V Voltage Divider     | Battery                    |
| Environment Sensor          | GY-68 BMP180              | I2C                        |
| Buzzer (optional)           | Active 5V                 | Alarm                      |

### Pinout (default)

```
ST7789:
  MOSI  → GPIO 11
  SCLK  → GPIO 12
  CS    → GPIO 10
  DC    → GPIO 9
  RST   → GPIO 8
  BL    → GPIO 13

Sensors:
  ZMPT101B   → GPIO 4  (ADC)
  ACS712     → GPIO 5  (ADC)
  DC Voltage → GPIO 6  (ADC)
  BMP180 SDA → GPIO 15
  BMP180 SCL → GPIO 16

Extras:
  Buzzer     → GPIO 7
  Status LED → GPIO 2
```

## Software Setup

### 1. Requirements
- [PlatformIO](https://platformio.org/) (VS Code / Cursor recommended)
- ESP32 Arduino core (auto installed by PlatformIO)

### 2. Configuration

Edit `include/config.h`:

```cpp
#define WIFI_SSID           "YourWiFi"
#define WIFI_PASSWORD       "YourPassword"
#define OWM_API_KEY         "your_openweathermap_api_key"
#define OWM_CITY            "Yangon"
#define OWM_COUNTRY_CODE    "MM"
```

### 3. Sensor Calibration (Important!)

After first run, measure real values and adjust in `config.h`:

```cpp
#define ZMPT_CALIBRATION    0.55f     // AC Voltage scale
#define ACS712_ZERO_OFFSET  1.65f     // Mid-point voltage of ACS712
#define ACS712_SENSITIVITY  0.100f    // 100mV/A for 20A module
#define DC_VOLT_DIVIDER     7.5f      // Voltage divider ratio
```

### 4. Build & Upload

```bash
pio run -t upload
pio device monitor
```

## Pages Overview

| Page | Name              | Content                              |
|------|-------------------|--------------------------------------|
| 1    | MAIN / INVERTER   | 3 gauges + Battery + Status          |
| 2    | BATTERY & DC      | Big % + Min/Max                      |
| 3    | AC DETAIL         | V, I, P, F, PF, Load Type            |
| 4    | POWER GRAPH       | Live 60s power history               |
| 5    | WEATHER           | OpenWeatherMap data                  |
| 6    | ENVIRONMENT       | BMP180 Temp/Pressure/Altitude        |
| 7    | ENERGY SUMMARY    | Today / Yesterday / Month / Total    |
| 8    | ALARMS            | Low Bat / High Temp / Overload       |
| 9    | SYSTEM STATUS     | WiFi, NTP, Sensors, Uptime           |
| 10   | MIN / MAX LOG     | Recorded extremes                    |
| 11   | DEVICE INFO       | MAC, IP, Board info                  |
| 12   | ABOUT             | Firmware version                     |

## Alarm Thresholds (default)

- **Low Battery** : < 11.5 V
- **High Temperature** : > 45 °C
- **Overload** : > 300 W

When any alarm triggers → jumps immediately to **Alarms** page + short beep.

## Power Factor Calculation

Uses interleaved sampling of ZMPT101B + ACS712, finds zero-crossing difference, then:

```
PF = cos(Δphase)
```

Fallback to 0.98 if detection fails.

## Energy Persistence

All kWh counters are stored in ESP32 **NVS** every 60 seconds and on important events. Data survives power loss / reboot.

## License

MIT License – free for personal and commercial use.

## Credits

- TFT_eSPI by Bodmer
- Adafruit BMP085 library
- OpenWeatherMap API
- Inspired by modern energy monitor UIs

---

**Developed for Smart Energy Systems – Myanmar**
