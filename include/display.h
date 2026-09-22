#pragma once
#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"
#include "sensors.h"

extern TFT_eSPI tft;

void displayBegin();
void displaySetBrightness(uint8_t percent);
void drawPage(int page);
void drawHeader(const char* title, bool showWifi = true, bool showTime = true);
void drawFooter();

// Individual pages
void drawPageMain();
void drawPageBattery();
void drawPageACDetail();
void drawPagePowerGraph();
void drawPageWeather();
void drawPageEnvironment();
void drawPageEnergy();
void drawPageAlarms();
void drawPageSystem();
void drawPageMinMax();
void drawPageDevice();
void drawPageAbout();

// Helpers
void drawArcGauge(int cx, int cy, int r, float value, float minV, float maxV, uint16_t color, const char* unit);
void drawBatteryIcon(int x, int y, float percent);
void drawProgressBar(int x, int y, int w, int h, float percent, uint16_t color);
void drawStatusBadge(int x, int y, const char* text, uint16_t bgColor);
String formatTime();
String formatDate();
void updatePowerHistory(float power);
float getPowerHistory(int index);  // 0 = oldest, 59 = newest (1 min history for graph)
