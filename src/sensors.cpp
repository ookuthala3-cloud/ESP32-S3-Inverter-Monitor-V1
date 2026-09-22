#include "sensors.h"
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>

SensorData sensors;
WeatherData weather;
Adafruit_BMP085 bmp;
Preferences preferences;

// Internal sampling buffers for PF calculation
static float voltageSamples[SAMPLE_COUNT];
static float currentSamples[SAMPLE_COUNT];
static int sampleIndex = 0;

// Energy tracking
static float energyAccumulatorWh = 0.0f;   // Wh since last save
static uint8_t lastDay = 0;
static uint8_t lastMonth = 0;

// -------------------- ADC Helpers --------------------
float readADCVoltage(int pin, int samples = 16) {
    uint32_t sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogReadMilliVolts(pin);
        delayMicroseconds(50);
    }
    return (sum / (float)samples) / 1000.0f;  // Volts
}

// -------------------- AC Voltage (ZMPT101B) --------------------
float readACVoltageRMS() {
    const int N = 200;
    float sumSquares = 0.0f;
    float offset = 1.65f;   // approximate mid-point (calibrate)

    for (int i = 0; i < N; i++) {
        float v = readADCVoltage(PIN_ZMPT101B, 1);
        float centered = v - offset;
        sumSquares += centered * centered;
        delayMicroseconds(200);  // ~5kHz sampling
    }

    float rms = sqrt(sumSquares / N);
    float voltage = rms * ZMPT_CALIBRATION * 230.0f / 1.0f;  // scale to real V
    // Simple clamp
    if (voltage < 5.0f) voltage = 0.0f;
    return voltage * (1.0f + ZMPT_OFFSET);
}

// -------------------- AC Current (ACS712 20A) --------------------
float readACCurrentRMS() {
    const int N = 200;
    float sumSquares = 0.0f;

    for (int i = 0; i < N; i++) {
        float v = readADCVoltage(PIN_ACS712, 1);
        float centered = v - ACS712_ZERO_OFFSET;
        sumSquares += centered * centered;
        delayMicroseconds(200);
    }

    float rms = sqrt(sumSquares / N);
    float current = rms / ACS712_SENSITIVITY;
    if (current < 0.05f) current = 0.0f;
    return current;
}

// -------------------- Power Factor (Zero-crossing method) --------------------
float calculatePowerFactor() {
    // Fast sample both channels
    const int N = SAMPLE_COUNT;
    float vOffset = 1.65f;
    float iOffset = ACS712_ZERO_OFFSET;

    // Collect interleaved samples
    for (int i = 0; i < N; i++) {
        voltageSamples[i] = readADCVoltage(PIN_ZMPT101B, 1) - vOffset;
        currentSamples[i] = readADCVoltage(PIN_ACS712, 1) - iOffset;
        delayMicroseconds(400);  // ~2.5 kHz
    }

    // Find zero crossings of voltage
    int vCross = -1, iCross = -1;
    for (int i = 1; i < N; i++) {
        if (voltageSamples[i-1] <= 0 && voltageSamples[i] > 0 && vCross < 0) {
            vCross = i;
        }
        if (currentSamples[i-1] <= 0 && currentSamples[i] > 0 && iCross < 0) {
            iCross = i;
        }
        if (vCross >= 0 && iCross >= 0) break;
    }

    if (vCross < 0 || iCross < 0) return 0.98f;  // fallback

    // Phase difference in samples → degrees
    int delta = iCross - vCross;
    // Handle wrap
    if (delta > N/2) delta -= N;
    if (delta < -N/2) delta += N;

    // Assuming ~50Hz and our sampling, convert to angle
    // Rough: full cycle ~ SAMPLE_COUNT samples at 50Hz
    float phaseDeg = (delta / (float)N) * 360.0f;
    float pf = cos(phaseDeg * PI / 180.0f);

    // Clamp realistic PF
    if (pf < 0.5f) pf = 0.5f;
    if (pf > 1.0f) pf = 1.0f;
    return pf;
}

// -------------------- Frequency (simple zero cross) --------------------
float measureFrequency() {
    // Count zero crossings in 200ms
    int crosses = 0;
    float prev = readADCVoltage(PIN_ZMPT101B, 1) - 1.65f;
    unsigned long start = millis();
    while (millis() - start < 200) {
        float now = readADCVoltage(PIN_ZMPT101B, 1) - 1.65f;
        if ((prev <= 0 && now > 0) || (prev >= 0 && now < 0)) {
            crosses++;
        }
        prev = now;
        delayMicroseconds(300);
    }
    // crosses / 2 = cycles in 0.2s → Hz
    float freq = (crosses / 2.0f) / 0.2f;
    if (freq < 40.0f || freq > 65.0f) freq = 50.0f;
    return freq;
}

// -------------------- Battery --------------------
float readBatteryVoltage() {
    float v = readADCVoltage(PIN_DC_VOLTAGE, 32);
    float battery = v * DC_VOLT_DIVIDER + DC_VOLT_OFFSET;
    if (battery < 0.5f) battery = 0.0f;
    return battery;
}

float calculateBatteryPercent(float voltage) {
    if (voltage <= BATTERY_MIN_V) return 0.0f;
    if (voltage >= BATTERY_MAX_V) return 100.0f;
    return (voltage - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V) * 100.0f;
}

// -------------------- BMP180 --------------------
void sensorsBegin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // 0-3.3V

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!bmp.begin()) {
        Serial.println("[BMP180] Sensor not found!");
    } else {
        Serial.println("[BMP180] OK");
    }

    // Init min/max
    sensors.minACVoltage = 999.0f;
    sensors.maxACVoltage = 0.0f;
    sensors.minACCurrent = 999.0f;
    sensors.maxACCurrent = 0.0f;
    sensors.minPower = 9999.0f;
    sensors.maxPower = 0.0f;
    sensors.minBatteryV = 999.0f;
    sensors.maxBatteryV = 0.0f;

    energyLoadFromNVS();
    weather.valid = false;
}

void sensorsUpdate() {
    // AC measurements
    sensors.acVoltageRMS = readACVoltageRMS();
    sensors.acCurrentRMS = readACCurrentRMS();
    sensors.powerFactor  = calculatePowerFactor();
    sensors.frequency    = measureFrequency();

    sensors.acPower = sensors.acVoltageRMS * sensors.acCurrentRMS * sensors.powerFactor;
    sensors.apparentPower = sensors.acVoltageRMS * sensors.acCurrentRMS;

    // Battery
    sensors.batteryVoltage = readBatteryVoltage();
    sensors.batteryPercent = calculateBatteryPercent(sensors.batteryVoltage);

    // BMP180
    sensors.temperature = bmp.readTemperature();
    sensors.pressure    = bmp.readPressure() / 100.0f;  // hPa
    sensors.altitude    = bmp.readAltitude(1013.25);
    sensors.seaLevelPressure = bmp.readSealevelPressure() / 100.0f;
    sensors.inverterTemp = sensors.temperature;  // same for now

    // Alarms
    sensors.lowBattery = (sensors.batteryVoltage < ALARM_LOW_BATTERY_V && sensors.batteryVoltage > 1.0f);
    sensors.highTemp   = (sensors.temperature > ALARM_HIGH_TEMP_C);
    sensors.overload   = (sensors.acPower > ALARM_OVERLOAD_W);
    sensors.systemNormal = !(sensors.lowBattery || sensors.highTemp || sensors.overload);

    // Min / Max update
    if (sensors.acVoltageRMS > 10.0f) {
        if (sensors.acVoltageRMS < sensors.minACVoltage) sensors.minACVoltage = sensors.acVoltageRMS;
        if (sensors.acVoltageRMS > sensors.maxACVoltage) sensors.maxACVoltage = sensors.acVoltageRMS;
    }
    if (sensors.acCurrentRMS < sensors.minACCurrent) sensors.minACCurrent = sensors.acCurrentRMS;
    if (sensors.acCurrentRMS > sensors.maxACCurrent) sensors.maxACCurrent = sensors.acCurrentRMS;
    if (sensors.acPower < sensors.minPower) sensors.minPower = sensors.acPower;
    if (sensors.acPower > sensors.maxPower) sensors.maxPower = sensors.acPower;
    if (sensors.batteryVoltage > 1.0f) {
        if (sensors.batteryVoltage < sensors.minBatteryV) sensors.minBatteryV = sensors.batteryVoltage;
        if (sensors.batteryVoltage > sensors.maxBatteryV) sensors.maxBatteryV = sensors.batteryVoltage;
    }

    sensors.lastUpdate = millis();
}

void sensorsResetMinMax() {
    sensors.minACVoltage = sensors.acVoltageRMS;
    sensors.maxACVoltage = sensors.acVoltageRMS;
    sensors.minACCurrent = sensors.acCurrentRMS;
    sensors.maxACCurrent = sensors.acCurrentRMS;
    sensors.minPower = sensors.acPower;
    sensors.maxPower = sensors.acPower;
    sensors.minBatteryV = sensors.batteryVoltage;
    sensors.maxBatteryV = sensors.batteryVoltage;
}

// -------------------- Energy (NVS) --------------------
void energyLoadFromNVS() {
    preferences.begin("energy", true);  // read-only
    sensors.powerToday     = preferences.getFloat("today", 0.0f);
    sensors.powerYesterday = preferences.getFloat("yesterday", 0.0f);
    sensors.powerMonth     = preferences.getFloat("month", 0.0f);
    sensors.powerTotal     = preferences.getFloat("total", 0.0f);
    lastDay   = preferences.getUChar("day", 0);
    lastMonth = preferences.getUChar("monthNum", 0);
    preferences.end();

    Serial.printf("[Energy] Loaded: Today=%.2f, Total=%.2f kWh\n",
                  sensors.powerToday, sensors.powerTotal);
}

void energySaveToNVS() {
    preferences.begin("energy", false);
    preferences.putFloat("today", sensors.powerToday);
    preferences.putFloat("yesterday", sensors.powerYesterday);
    preferences.putFloat("month", sensors.powerMonth);
    preferences.putFloat("total", sensors.powerTotal);
    preferences.putUChar("day", lastDay);
    preferences.putUChar("monthNum", lastMonth);
    preferences.end();
}

void energyUpdate(float powerWatts, float dtSeconds) {
    if (powerWatts < 1.0f) return;

    float deltaWh = powerWatts * (dtSeconds / 3600.0f);
    energyAccumulatorWh += deltaWh;

    sensors.powerToday += deltaWh / 1000.0f;
    sensors.powerMonth += deltaWh / 1000.0f;
    sensors.powerTotal += deltaWh / 1000.0f;

    // Day change detection (simple - relies on NTP later)
    // For now we just accumulate. Full day rollover needs time sync.
}

// -------------------- Weather (OpenWeatherMap) --------------------
void weatherBegin() {
    weather.valid = false;
}

void weatherUpdate() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=";
    url += OWM_CITY;
    url += ",";
    url += OWM_COUNTRY_CODE;
    url += "&appid=";
    url += OWM_API_KEY;
    url += "&units=";
    url += OWM_UNITS;

    http.begin(url);
    int code = http.GET();

    if (code == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err) {
            weather.temp        = doc["main"]["temp"];
            weather.humidity    = doc["main"]["humidity"];
            weather.pressure    = doc["main"]["pressure"];
            weather.windSpeed   = doc["wind"]["speed"];
            weather.weatherID   = doc["weather"][0]["id"];
            weather.description = doc["weather"][0]["description"].as<String>();
            weather.icon        = doc["weather"][0]["icon"].as<String>();
            weather.valid       = true;
            weather.lastUpdate  = millis();
            Serial.println("[Weather] Updated OK");
        }
    } else {
        Serial.printf("[Weather] HTTP error: %d\n", code);
    }
    http.end();
}
