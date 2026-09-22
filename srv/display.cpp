#include "display.h"
#include <WiFi.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

// Power history ring buffer (60 points ≈ 1 minute at 1s interval)
static float powerHistory[60];
static int historyIndex = 0;
static bool historyFilled = false;

// Colors
#define COLOR_BG        TFT_BLACK
#define COLOR_TITLE     TFT_CYAN
#define COLOR_VALUE     TFT_WHITE
#define COLOR_UNIT      TFT_LIGHTGREY
#define COLOR_GREEN     0x07E0
#define COLOR_YELLOW    0xFFE0
#define COLOR_ORANGE    0xFD20
#define COLOR_RED       0xF800
#define COLOR_BLUE      0x001F
#define COLOR_DARKGREY  0x4208

void displayBegin() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(TL_DATUM);

    // Clear history
    for (int i = 0; i < 60; i++) powerHistory[i] = 0;
}

void displaySetBrightness(uint8_t percent) {
    // Simple on/off for now (PWM can be added)
    digitalWrite(TFT_BL, percent > 0 ? HIGH : LOW);
}

String formatTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "--:--:--";
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    return String(buf);
}

String formatDate() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "-- --- ----";
    char buf[20];
    strftime(buf, sizeof(buf), "%d %b %Y", &timeinfo);
    return String(buf);
}

void updatePowerHistory(float power) {
    powerHistory[historyIndex] = power;
    historyIndex = (historyIndex + 1) % 60;
    if (historyIndex == 0) historyFilled = true;
}

float getPowerHistory(int index) {
    // index 0 = oldest
    int realIdx = (historyIndex + index) % 60;
    if (!historyFilled && realIdx >= historyIndex) return 0;
    return powerHistory[realIdx];
}

void drawHeader(const char* title, bool showWifi, bool showTime) {
    tft.fillRect(0, 0, 240, 22, 0x1082);  // dark blue
    tft.setTextColor(COLOR_TITLE, 0x1082);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(title, 120, 4, 2);

    if (showTime) {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, 0x1082);
        tft.drawString(formatDate(), 4, 4, 1);
    }
    if (showWifi) {
        tft.setTextDatum(TR_DATUM);
        if (WiFi.status() == WL_CONNECTED) {
            tft.setTextColor(COLOR_GREEN, 0x1082);
            tft.drawString("WiFi", 236, 4, 1);
        } else {
            tft.setTextColor(COLOR_RED, 0x1082);
            tft.drawString("NoWiFi", 236, 4, 1);
        }
    }
}

void drawFooter() {
    // small page indicator dots can be added if needed
}

// -------------------- Arc Gauge --------------------
void drawArcGauge(int cx, int cy, int r, float value, float minV, float maxV, uint16_t color, const char* label) {
    float ratio = constrain((value - minV) / (maxV - minV), 0.0f, 1.0f);
    int startAngle = 135;
    int endAngle = 135 + (int)(ratio * 270);

    // Background arc
    for (int a = 135; a <= 405; a += 3) {
        float rad = a * PI / 180.0f;
        int x1 = cx + cos(rad) * (r - 4);
        int y1 = cy + sin(rad) * (r - 4);
        int x2 = cx + cos(rad) * r;
        int y2 = cy + sin(rad) * r;
        tft.drawLine(x1, y1, x2, y2, COLOR_DARKGREY);
    }

    // Value arc
    for (int a = 135; a <= endAngle; a += 2) {
        float rad = a * PI / 180.0f;
        int x1 = cx + cos(rad) * (r - 5);
        int y1 = cy + sin(rad) * (r - 5);
        int x2 = cx + cos(rad) * r;
        int y2 = cy + sin(rad) * r;
        tft.drawLine(x1, y1, x2, y2, color);
    }

    // Center text
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    char buf[16];
    dtostrf(value, 4, 1, buf);
    tft.drawString(buf, cx, cy - 8, 4);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString(label, cx, cy + 16, 2);
}

void drawBatteryIcon(int x, int y, float percent) {
    int w = 28, h = 14;
    tft.drawRect(x, y, w, h, TFT_WHITE);
    tft.fillRect(x + w, y + 3, 3, 8, TFT_WHITE);  // tip

    int fillW = (int)((w - 4) * percent / 100.0f);
    uint16_t col = percent > 50 ? COLOR_GREEN : (percent > 20 ? COLOR_YELLOW : COLOR_RED);
    if (fillW > 0) tft.fillRect(x + 2, y + 2, fillW, h - 4, col);
}

void drawProgressBar(int x, int y, int w, int h, float percent, uint16_t color) {
    tft.drawRect(x, y, w, h, TFT_WHITE);
    int fill = (int)((w - 2) * constrain(percent, 0, 100) / 100.0f);
    if (fill > 0) tft.fillRect(x + 1, y + 1, fill, h - 2, color);
}

void drawStatusBadge(int x, int y, const char* text, uint16_t bgColor) {
    int tw = tft.textWidth(text, 2);
    tft.fillRoundRect(x, y, tw + 12, 18, 4, bgColor);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_BLACK, bgColor);
    tft.drawString(text, x + 6, y + 2, 2);
}

// ============================================================
// PAGE 1 - MAIN / INVERTER
// ============================================================
void drawPageMain() {
    tft.fillScreen(COLOR_BG);
    drawHeader("MAIN / INVERTER");

    // Three gauges
    drawArcGauge(50, 90, 38, sensors.acVoltageRMS, 0, 300, COLOR_GREEN, "V");
    drawArcGauge(120, 90, 38, sensors.acCurrentRMS, 0, 20, COLOR_BLUE, "A");
    drawArcGauge(190, 90, 38, sensors.acPower, 0, 500, COLOR_ORANGE, "W");

    // Labels under gauges
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("AC OUT", 50, 135, 1);
    tft.drawString("CURRENT", 120, 135, 1);
    tft.drawString("POWER", 190, 135, 1);

    // Frequency & PF
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    char buf[32];
    sprintf(buf, "%.1f Hz", sensors.frequency);
    tft.drawString(buf, 20, 155, 2);
    sprintf(buf, "PF %.2f", sensors.powerFactor);
    tft.drawString(buf, 140, 155, 2);

    // Battery bar
    drawBatteryIcon(15, 185, sensors.batteryPercent);
    sprintf(buf, "%.1fV  %.0f%%", sensors.batteryVoltage, sensors.batteryPercent);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    tft.drawString(buf, 50, 185, 2);

    // System status
    if (sensors.systemNormal) {
        drawStatusBadge(150, 183, "NORMAL", COLOR_GREEN);
    } else {
        drawStatusBadge(150, 183, "ALARM", COLOR_RED);
    }

    // Temp
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    sprintf(buf, "Inv Temp: %.1f C", sensors.inverterTemp);
    tft.drawString(buf, 20, 215, 1);
}

// ============================================================
// PAGE 2 - BATTERY & DC
// ============================================================
void drawPageBattery() {
    tft.fillScreen(COLOR_BG);
    drawHeader("BATTERY & DC");

    // Big percent
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    char buf[16];
    sprintf(buf, "%.0f%%", sensors.batteryPercent);
    tft.drawString(buf, 120, 50, 7);

    // Voltage
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.2f V", sensors.batteryVoltage);
    tft.drawString(buf, 120, 110, 4);

    // Min Max Avg
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("MIN", 30, 150, 1);
    tft.drawString("MAX", 100, 150, 1);
    tft.drawString("AVG", 170, 150, 1);

    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.1f", sensors.minBatteryV);
    tft.drawString(buf, 30, 165, 2);
    sprintf(buf, "%.1f", sensors.maxBatteryV);
    tft.drawString(buf, 100, 165, 2);
    sprintf(buf, "%.1f", (sensors.minBatteryV + sensors.maxBatteryV) / 2);
    tft.drawString(buf, 170, 165, 2);

    // Status
    tft.setTextDatum(TC_DATUM);
    if (sensors.lowBattery) {
        drawStatusBadge(70, 200, "LOW BATTERY", COLOR_RED);
    } else {
        drawStatusBadge(70, 200, "NORMAL", COLOR_GREEN);
    }
}

// ============================================================
// PAGE 3 - AC DETAIL
// ============================================================
void drawPageACDetail() {
    tft.fillScreen(COLOR_BG);
    drawHeader("AC DETAILS");

    tft.setTextDatum(TL_DATUM);
    char buf[32];

    // Voltage
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Voltage (RMS)", 15, 35, 2);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.1f V", sensors.acVoltageRMS);
    tft.drawString(buf, 15, 55, 4);

    // Frequency
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Frequency", 140, 35, 2);
    tft.setTextColor(0xFFE0, COLOR_BG);
    sprintf(buf, "%.1f Hz", sensors.frequency);
    tft.drawString(buf, 140, 55, 4);

    // Current
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Current (RMS)", 15, 100, 2);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.2f A", sensors.acCurrentRMS);
    tft.drawString(buf, 15, 120, 4);

    // Power
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Power", 140, 100, 2);
    tft.setTextColor(COLOR_ORANGE, COLOR_BG);
    sprintf(buf, "%.0f W", sensors.acPower);
    tft.drawString(buf, 140, 120, 4);

    // PF & Load type
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Power Factor", 15, 165, 2);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    sprintf(buf, "%.2f", sensors.powerFactor);
    tft.drawString(buf, 15, 185, 4);

    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Load Type", 140, 165, 2);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    if (sensors.powerFactor > 0.95f) tft.drawString("RESISTIVE", 140, 185, 2);
    else if (sensors.powerFactor > 0.8f) tft.drawString("MIXED", 140, 185, 2);
    else tft.drawString("INDUCTIVE", 140, 185, 2);
}

// ============================================================
// PAGE 4 - POWER GRAPH
// ============================================================
void drawPagePowerGraph() {
    tft.fillScreen(COLOR_BG);
    drawHeader("LIVE POWER (W)");

    // Max / Avg
    float maxP = 0, sumP = 0;
    int count = historyFilled ? 60 : historyIndex;
    for (int i = 0; i < count; i++) {
        float p = getPowerHistory(i);
        if (p > maxP) maxP = p;
        sumP += p;
    }
    float avgP = count > 0 ? sumP / count : 0;

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    char buf[24];
    sprintf(buf, "MAX %.0f W", maxP);
    tft.drawString(buf, 230, 28, 1);
    sprintf(buf, "AVG %.0f W", avgP);
    tft.drawString(buf, 230, 42, 1);

    // Graph area
    int gx = 20, gy = 60, gw = 200, gh = 140;
    tft.drawRect(gx, gy, gw, gh, COLOR_DARKGREY);

    // Y scale
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("1000", gx - 2, gy, 1);
    tft.drawString("500", gx - 2, gy + gh/2 - 4, 1);
    tft.drawString("0", gx - 2, gy + gh - 8, 1);

    // Draw line
    if (count > 1) {
        for (int i = 1; i < count; i++) {
            int x1 = gx + (i - 1) * gw / 59;
            int x2 = gx + i * gw / 59;
            float p1 = getPowerHistory(i - 1);
            float p2 = getPowerHistory(i);
            int y1 = gy + gh - (int)(constrain(p1, 0, 1000) / 1000.0f * gh);
            int y2 = gy + gh - (int)(constrain(p2, 0, 1000) / 1000.0f * gh);
            tft.drawLine(x1, y1, x2, y2, COLOR_ORANGE);
        }
    }

    // Time labels
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("-60s", gx, gy + gh + 5, 1);
    tft.drawString("-30s", gx + gw/2, gy + gh + 5, 1);
    tft.drawString("0s", gx + gw, gy + gh + 5, 1);
}

// ============================================================
// PAGE 5 - WEATHER
// ============================================================
void drawPageWeather() {
    tft.fillScreen(COLOR_BG);
    drawHeader("WEATHER");

    if (!weather.valid) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_YELLOW, COLOR_BG);
        tft.drawString("Waiting for", 120, 100, 2);
        tft.drawString("Weather data...", 120, 125, 2);
        return;
    }

    // Temp big
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    char buf[16];
    sprintf(buf, "%.0f C", weather.temp);
    tft.drawString(buf, 120, 50, 7);

    // Description
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString(weather.description.c_str(), 120, 110, 2);

    // Details boxes
    int y = 145;
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Humidity", 20, y, 1);
    tft.drawString("Pressure", 90, y, 1);
    tft.drawString("Wind", 160, y, 1);

    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.0f%%", weather.humidity);
    tft.drawString(buf, 20, y + 15, 2);
    sprintf(buf, "%.0f", weather.pressure);
    tft.drawString(buf, 90, y + 15, 2);
    sprintf(buf, "%.1f", weather.windSpeed);
    tft.drawString(buf, 160, y + 15, 2);

    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("hPa", 90, y + 35, 1);
    tft.drawString("m/s", 160, y + 35, 1);

    // Updated
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_DARKGREY, COLOR_BG);
    tft.drawString("OpenWeatherMap", 120, 220, 1);
}

// ============================================================
// PAGE 6 - ENVIRONMENT (BMP180)
// ============================================================
void drawPageEnvironment() {
    tft.fillScreen(COLOR_BG);
    drawHeader("ENVIRONMENT");

    char buf[24];
    tft.setTextDatum(TL_DATUM);

    // Temperature
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Temperature", 20, 40, 2);
    tft.setTextColor(COLOR_ORANGE, COLOR_BG);
    sprintf(buf, "%.1f C", sensors.temperature);
    tft.drawString(buf, 20, 60, 4);

    // Pressure
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Pressure", 20, 110, 2);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.1f hPa", sensors.pressure);
    tft.drawString(buf, 20, 130, 4);

    // Altitude
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Altitude", 20, 180, 2);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.0f m", sensors.altitude);
    tft.drawString(buf, 20, 200, 4);

    // Sea level
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    sprintf(buf, "SLP: %.1f hPa", sensors.seaLevelPressure);
    tft.drawString(buf, 230, 220, 1);
}

// ============================================================
// PAGE 7 - ENERGY SUMMARY
// ============================================================
void drawPageEnergy() {
    tft.fillScreen(COLOR_BG);
    drawHeader("ENERGY SUMMARY");

    char buf[24];
    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Today (kWh)", 20, 40, 2);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    sprintf(buf, "%.2f", sensors.powerToday);
    tft.drawString(buf, 20, 60, 4);

    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Yesterday (kWh)", 20, 110, 2);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.2f", sensors.powerYesterday);
    tft.drawString(buf, 20, 130, 4);

    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("This Month (kWh)", 20, 170, 2);
    tft.setTextColor(COLOR_ORANGE, COLOR_BG);
    sprintf(buf, "%.2f", sensors.powerMonth);
    tft.drawString(buf, 20, 190, 4);

    // Total at bottom
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("TOTAL", 120, 215, 1);
    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    sprintf(buf, "%.2f kWh", sensors.powerTotal);
    tft.drawString(buf, 120, 225, 2);
}

// ============================================================
// PAGE 8 - ALARMS
// ============================================================
void drawPageAlarms() {
    tft.fillScreen(COLOR_BG);
    drawHeader("ALARMS");

    int y = 40;
    char buf[32];

    // Low Battery
    if (sensors.lowBattery) {
        tft.fillRoundRect(10, y, 220, 35, 5, 0x8000);
        tft.setTextColor(COLOR_RED, 0x8000);
    } else {
        tft.fillRoundRect(10, y, 220, 35, 5, 0x2104);
        tft.setTextColor(COLOR_GREEN, 0x2104);
    }
    tft.setTextDatum(TL_DATUM);
    tft.drawString("LOW BATTERY", 20, y + 5, 2);
    sprintf(buf, "%.1f V", sensors.batteryVoltage);
    tft.drawString(buf, 160, y + 5, 2);
    y += 45;

    // High Temp
    if (sensors.highTemp) {
        tft.fillRoundRect(10, y, 220, 35, 5, 0x8000);
        tft.setTextColor(COLOR_RED, 0x8000);
    } else {
        tft.fillRoundRect(10, y, 220, 35, 5, 0x2104);
        tft.setTextColor(COLOR_GREEN, 0x2104);
    }
    tft.drawString("HIGH TEMP", 20, y + 5, 2);
    sprintf(buf, "%.1f C", sensors.temperature);
    tft.drawString(buf, 160, y + 5, 2);
    y += 45;

    // Overload
    if (sensors.overload) {
        tft.fillRoundRect(10, y, 220, 35, 5, 0x8000);
        tft.setTextColor(COLOR_RED, 0x8000);
    } else {
        tft.fillRoundRect(10, y, 220, 35, 5, 0x2104);
        tft.setTextColor(COLOR_GREEN, 0x2104);
    }
    tft.drawString("OVERLOAD", 20, y + 5, 2);
    sprintf(buf, "%.0f W", sensors.acPower);
    tft.drawString(buf, 160, y + 5, 2);
    y += 50;

    // System status
    tft.setTextDatum(TC_DATUM);
    if (sensors.systemNormal) {
        drawStatusBadge(60, y, "SYSTEM NORMAL", COLOR_GREEN);
    } else {
        drawStatusBadge(70, y, "CHECK ALARMS", COLOR_RED);
    }
}

// ============================================================
// PAGE 9 - SYSTEM STATUS
// ============================================================
void drawPageSystem() {
    tft.fillScreen(COLOR_BG);
    drawHeader("SYSTEM STATUS");

    tft.setTextDatum(TL_DATUM);
    int y = 40;
    char buf[32];

    auto drawRow = [&](const char* label, const char* value, uint16_t color) {
        tft.setTextColor(COLOR_UNIT, COLOR_BG);
        tft.drawString(label, 20, y, 2);
        tft.setTextColor(color, COLOR_BG);
        tft.drawString(value, 130, y, 2);
        y += 28;
    };

    drawRow("Wi-Fi", WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED",
            WiFi.status() == WL_CONNECTED ? COLOR_GREEN : COLOR_RED);

    struct tm timeinfo;
    bool timeOk = getLocalTime(&timeinfo);
    drawRow("NTP Time", timeOk ? "SYNCED" : "NOT SYNCED", timeOk ? COLOR_GREEN : COLOR_YELLOW);

    drawRow("Weather API", weather.valid ? "UPDATED" : "WAITING", weather.valid ? COLOR_GREEN : COLOR_YELLOW);
    drawRow("Sensors", "OK", COLOR_GREEN);
    drawRow("Buzzer", "ON", COLOR_GREEN);

    // Uptime
    unsigned long sec = millis() / 1000;
    sprintf(buf, "%02lu:%02lu:%02lu", sec / 3600, (sec % 3600) / 60, sec % 60);
    drawRow("Uptime", buf, COLOR_VALUE);

    drawRow("Firmware", "V1.0.0", COLOR_VALUE);
}

// ============================================================
// PAGE 10 - MIN / MAX LOG
// ============================================================
void drawPageMinMax() {
    tft.fillScreen(COLOR_BG);
    drawHeader("MIN / MAX LOG");

    tft.setTextDatum(TL_DATUM);
    char buf[32];
    int y = 40;

    auto drawMinMax = [&](const char* label, float mn, float mx, const char* unit) {
        tft.setTextColor(COLOR_UNIT, COLOR_BG);
        tft.drawString(label, 15, y, 2);
        tft.setTextColor(COLOR_VALUE, COLOR_BG);
        sprintf(buf, "%.1f", mn);
        tft.drawString(buf, 110, y, 2);
        sprintf(buf, "%.1f %s", mx, unit);
        tft.drawString(buf, 165, y, 2);
        y += 30;
    };

    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("          Min     Max", 15, y, 1);
    y += 20;

    drawMinMax("AC Voltage", sensors.minACVoltage, sensors.maxACVoltage, "V");
    drawMinMax("AC Current", sensors.minACCurrent, sensors.maxACCurrent, "A");
    drawMinMax("Power", sensors.minPower, sensors.maxPower, "W");
    drawMinMax("Battery", sensors.minBatteryV, sensors.maxBatteryV, "V");

    // Reset hint
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_DARKGREY, COLOR_BG);
    tft.drawString("Reset via serial or reboot", 120, 220, 1);
}

// ============================================================
// PAGE 11 - DEVICE INFO
// ============================================================
void drawPageDevice() {
    tft.fillScreen(COLOR_BG);
    drawHeader("DEVICE INFO");

    tft.setTextDatum(TL_DATUM);
    int y = 40;
    char buf[40];

    auto drawInfo = [&](const char* label, const char* value) {
        tft.setTextColor(COLOR_UNIT, COLOR_BG);
        tft.drawString(label, 15, y, 2);
        tft.setTextColor(COLOR_VALUE, COLOR_BG);
        tft.drawString(value, 110, y, 2);
        y += 26;
    };

    drawInfo("Device", "INV-MONITOR");
    drawInfo("Board", "ESP32-S3");
    drawInfo("Flash", "16 MB");
    drawInfo("PSRAM", "8 MB");
    drawInfo("LCD", "ST7789 240x240");

    // MAC
    String mac = WiFi.macAddress();
    drawInfo("MAC", mac.c_str());

    // IP
    if (WiFi.status() == WL_CONNECTED) {
        sprintf(buf, "%s", WiFi.localIP().toString().c_str());
        drawInfo("IP", buf);
    } else {
        drawInfo("IP", "---");
    }
}

// ============================================================
// PAGE 12 - ABOUT
// ============================================================
void drawPageAbout() {
    tft.fillScreen(COLOR_BG);
    drawHeader("ABOUT");

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_TITLE, COLOR_BG);
    tft.drawString("ESP32-S3", 120, 60, 4);
    tft.drawString("INVERTER MONITOR", 120, 95, 2);

    tft.setTextColor(COLOR_VALUE, COLOR_BG);
    tft.drawString("Firmware V1.0.0", 120, 140, 2);

    tft.setTextColor(COLOR_UNIT, COLOR_BG);
    tft.drawString("Build: Sep 2026", 120, 170, 1);
    tft.drawString("Developed for", 120, 195, 1);
    tft.drawString("Smart Energy System", 120, 210, 1);
}

// ============================================================
// Main draw dispatcher
// ============================================================
void drawPage(int page) {
    switch (page) {
        case PAGE_MAIN:        drawPageMain(); break;
        case PAGE_BATTERY:     drawPageBattery(); break;
        case PAGE_AC_DETAIL:   drawPageACDetail(); break;
        case PAGE_POWER_GRAPH: drawPagePowerGraph(); break;
        case PAGE_WEATHER:     drawPageWeather(); break;
        case PAGE_ENVIRONMENT: drawPageEnvironment(); break;
        case PAGE_ENERGY:      drawPageEnergy(); break;
        case PAGE_ALARMS:      drawPageAlarms(); break;
        case PAGE_SYSTEM:      drawPageSystem(); break;
        case PAGE_MINMAX:      drawPageMinMax(); break;
        case PAGE_DEVICE:      drawPageDevice(); break;
        case PAGE_ABOUT:       drawPageAbout(); break;
        default:               drawPageMain(); break;
    }
}
