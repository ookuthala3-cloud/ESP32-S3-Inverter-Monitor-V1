#pragma once

// ============================================================
// ESP32-S3 Inverter Monitor - Configuration
// ============================================================

// -------------------- WiFi & API --------------------
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"

// OpenWeatherMap
#define OWM_API_KEY         "YOUR_OPENWEATHERMAP_API_KEY"
#define OWM_CITY            "Yangon"          // City name
#define OWM_COUNTRY_CODE    "MM"              // Country code
#define OWM_UNITS           "metric"          // metric / imperial
#define WEATHER_UPDATE_INTERVAL  600000UL    // 10 minutes

// -------------------- Pin Definitions --------------------
// TFT ST7789 240x240 (SPI)
#define TFT_MOSI            11
#define TFT_SCLK            12
#define TFT_CS              10
#define TFT_DC              9
#define TFT_RST             8
#define TFT_BL              13

// Sensors (ADC1 - safe while WiFi active)
#define PIN_ZMPT101B        4     // AC Voltage
#define PIN_ACS712          5     // AC Current
#define PIN_DC_VOLTAGE      6     // Battery Voltage 0-25V

// I2C BMP180
#define PIN_I2C_SDA         15
#define PIN_I2C_SCL         16

// Optional
#define PIN_BUZZER          7
#define PIN_STATUS_LED      2

// -------------------- Sensor Calibration --------------------
// ZMPT101B (AC Voltage)
// Typical: 230V AC → ~1.5-2.5V peak at ADC after module
#define ZMPT_CALIBRATION    0.55f     // Adjust after calibration
#define ZMPT_OFFSET         0.0f

// ACS712 20A version: 100 mV/A , 2.5V at 0A (ESP32 ADC 3.3V)
#define ACS712_SENSITIVITY  0.100f    // V/A for 20A module
#define ACS712_ZERO_OFFSET  1.65f     // Mid-point voltage (calibrate!)

// DC Voltage Sensor 0-25V (voltage divider)
// Usually 5:1 or similar → max 25V → ~3.3V at ADC
#define DC_VOLT_DIVIDER     7.5f      // Adjust according to your module
#define DC_VOLT_OFFSET      0.0f

// -------------------- Battery 12V System --------------------
#define BATTERY_MIN_V       11.5f     // 0%
#define BATTERY_MAX_V       13.8f     // 100%
#define BATTERY_NOMINAL_V   12.6f

// -------------------- Alarm Thresholds --------------------
#define ALARM_LOW_BATTERY_V     11.5f
#define ALARM_HIGH_TEMP_C       45.0f
#define ALARM_OVERLOAD_W        300.0f

// -------------------- Timing --------------------
#define PAGE_ROTATE_MS          15000UL   // 15 seconds
#define SENSOR_SAMPLE_MS        200UL
#define DISPLAY_UPDATE_MS       500UL
#define ENERGY_SAVE_INTERVAL    60000UL   // Save kWh every 1 min

// -------------------- Display --------------------
#define SCREEN_WIDTH            240
#define SCREEN_HEIGHT           240
#define TOTAL_PAGES              12

// Page indices
enum PageID {
    PAGE_MAIN = 0,
    PAGE_BATTERY,
    PAGE_AC_DETAIL,
    PAGE_POWER_GRAPH,
    PAGE_WEATHER,
    PAGE_ENVIRONMENT,
    PAGE_ENERGY,
    PAGE_ALARMS,
    PAGE_SYSTEM,
    PAGE_MINMAX,
    PAGE_DEVICE,
    PAGE_ABOUT
};

// -------------------- Sampling for PF --------------------
#define SAMPLE_COUNT            128       // samples per cycle calc
#define SAMPLE_FREQ_HZ          2000      // target sampling rate
