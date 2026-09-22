#pragma once
#include <Arduino.h>
#include "config.h"

struct SensorData {
    // AC Side
    float acVoltageRMS;      // V
    float acCurrentRMS;      // A
    float acPower;           // W
    float powerFactor;       // 0.0 - 1.0
    float frequency;         // Hz
    float apparentPower;     // VA

    // DC / Battery
    float batteryVoltage;    // V
    float batteryPercent;    // %
    float dischargeCurrent;  // A (if available, else 0)

    // Environment (BMP180)
    float temperature;       // °C
    float pressure;          // hPa
    float altitude;          // m
    float seaLevelPressure;  // hPa

    // Inverter internal (estimate)
    float inverterTemp;      // °C (can use BMP or external)

    // Energy
    float powerToday;        // kWh
    float powerYesterday;    // kWh
    float powerMonth;        // kWh
    float powerTotal;        // kWh

    // Status
    bool systemNormal;
    bool lowBattery;
    bool highTemp;
    bool overload;

    // Min / Max logs
    float minACVoltage;
    float maxACVoltage;
    float minACCurrent;
    float maxACCurrent;
    float minPower;
    float maxPower;
    float minBatteryV;
    float maxBatteryV;

    unsigned long lastUpdate;
};

struct WeatherData {
    float temp;
    float humidity;
    float pressure;
    float windSpeed;
    int   weatherID;
    String description;
    String icon;
    bool  valid;
    unsigned long lastUpdate;
};

// Global instances
extern SensorData sensors;
extern WeatherData weather;

// Functions
void sensorsBegin();
void sensorsUpdate();
void sensorsResetMinMax();
void energyLoadFromNVS();
void energySaveToNVS();
void energyUpdate(float powerWatts, float dtSeconds);

// Weather
void weatherBegin();
void weatherUpdate();

// Helpers
float readACVoltageRMS();
float readACCurrentRMS();
float calculatePowerFactor();
float readBatteryVoltage();
float calculateBatteryPercent(float voltage);
