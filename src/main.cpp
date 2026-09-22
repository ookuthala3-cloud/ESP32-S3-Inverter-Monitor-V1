#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "sensors.h"
#include "display.h"

// -------------------- Globals --------------------
int currentPage = PAGE_MAIN;
unsigned long lastPageChange = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastEnergySave = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastHistoryUpdate = 0;
unsigned long lastEnergyTick = 0;

bool alarmActive = false;
int previousPageBeforeAlarm = PAGE_MAIN;

// -------------------- WiFi & NTP --------------------
void connectWiFi() {
    Serial.print("[WiFi] Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());

        // NTP
        configTime(6 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // UTC+6 Myanmar
        Serial.println("[NTP] Time sync started");
    } else {
        Serial.println("\n[WiFi] Failed - continuing offline");
    }
}

// -------------------- Setup --------------------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== ESP32-S3 Inverter Monitor v1.0.0 ===");

    // Status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);

    // Buzzer
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    // Display first
    displayBegin();
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("ESP32-S3", 120, 90, 4);
    tft.drawString("Inverter Monitor", 120, 125, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Starting...", 120, 170, 2);

    // Sensors
    sensorsBegin();

    // WiFi
    connectWiFi();

    // Weather
    weatherBegin();
    if (WiFi.status() == WL_CONNECTED) {
        weatherUpdate();
        lastWeatherUpdate = millis();
    }

    lastPageChange = millis();
    lastSensorUpdate = millis();
    lastDisplayUpdate = millis();
    lastEnergyTick = millis();
    lastHistoryUpdate = millis();

    Serial.println("[Setup] Complete");
    digitalWrite(PIN_STATUS_LED, LOW);
}

// -------------------- Loop --------------------
void loop() {
    unsigned long now = millis();

    // --- Sensor update ---
    if (now - lastSensorUpdate >= SENSOR_SAMPLE_MS) {
        sensorsUpdate();
        lastSensorUpdate = now;

        // Energy accumulation
        float dt = (now - lastEnergyTick) / 1000.0f;
        energyUpdate(sensors.acPower, dt);
        lastEnergyTick = now;

        // Alarm check
        bool anyAlarm = sensors.lowBattery || sensors.highTemp || sensors.overload;
        if (anyAlarm && !alarmActive) {
            // Enter alarm mode
            alarmActive = true;
            previousPageBeforeAlarm = currentPage;
            currentPage = PAGE_ALARMS;
            lastPageChange = now;

            // Beep
            digitalWrite(PIN_BUZZER, HIGH);
            delay(100);
            digitalWrite(PIN_BUZZER, LOW);
        } else if (!anyAlarm && alarmActive) {
            alarmActive = false;
            currentPage = previousPageBeforeAlarm;
            lastPageChange = now;
        }
    }

    // --- Power history (every 1 second) ---
    if (now - lastHistoryUpdate >= 1000) {
        updatePowerHistory(sensors.acPower);
        lastHistoryUpdate = now;
    }

    // --- Energy save to NVS ---
    if (now - lastEnergySave >= ENERGY_SAVE_INTERVAL) {
        energySaveToNVS();
        lastEnergySave = now;
    }

    // --- Weather update ---
    if (WiFi.status() == WL_CONNECTED && now - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
        weatherUpdate();
        lastWeatherUpdate = now;
    }

    // --- Page rotation (only if no active alarm) ---
    if (!alarmActive && now - lastPageChange >= PAGE_ROTATE_MS) {
        currentPage = (currentPage + 1) % TOTAL_PAGES;
        lastPageChange = now;
    }

    // --- Display update ---
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
        drawPage(currentPage);
        lastDisplayUpdate = now;
    }

    // Status LED blink
    static unsigned long lastBlink = 0;
    if (now - lastBlink > 1000) {
        digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
        lastBlink = now;
    }

    // Serial debug (optional)
    static unsigned long lastSerial = 0;
    if (now - lastSerial > 5000) {
        Serial.printf("V:%.1f I:%.2f P:%.0f PF:%.2f Bat:%.2f%% Temp:%.1f Page:%d\n",
                      sensors.acVoltageRMS, sensors.acCurrentRMS, sensors.acPower,
                      sensors.powerFactor, sensors.batteryPercent, sensors.temperature,
                      currentPage);
        lastSerial = now;
    }

    delay(10);
}
