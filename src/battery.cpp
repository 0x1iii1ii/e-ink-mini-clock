//
// Created by Li on 3/12/2026.
//

#include "battery.h"

#define ADC_MAX     4095.0f
#define ADC_VREF    3.3f     // adjust if you measure actual Vref (often ~3.1–3.3V)
#define DIV_RATIO   2.0f     // 100k / 100k divider

void battery_init() {
    pinMode(BATTRY_ADC, INPUT);
}

uint8_t readBattery() {
    // analogReadMilliVolts() already returns mV, just average it
    int32_t raw_mv = 0;
    for (int i = 0; i < 16; i++) {
        raw_mv += analogReadMilliVolts(BATTRY_ADC);
        delay(2);
    }
    raw_mv /= 16;
    // Serial.printf("Raw ADC: %ld mV\n", raw_mv);
    // Serial.printf("ADC pin voltage: %ld mV\n", raw_mv);       // should be ~1650 mV
    // Serial.printf("After ratio x2: %.2f V\n", raw_mv / 1000.0f * DIV_RATIO);
    // Scale back up through the voltage divider
    float v_batt = (raw_mv / 1000.0f) * DIV_RATIO;
    Serial.printf("Voltage: %.2f V\n", v_batt);

    // Clamp
    if (v_batt < 3.0f) v_batt = 3.0f;
    if (v_batt > 4.2f) v_batt = 4.2f;

    // Nonlinear LiPo curve
    float pct;
    if (v_batt >= 4.1f)      pct = 97.0f + (v_batt - 4.1f) * 50.0f;
    else if (v_batt >= 3.9f) pct = 75.0f + (v_batt - 3.9f) * 100.0f;
    else if (v_batt >= 3.7f) pct = 40.0f + (v_batt - 3.7f) * 175.0f;
    else if (v_batt >= 3.5f) pct = 15.0f + (v_batt - 3.5f) * 125.0f;
    else                     pct = (v_batt - 3.0f) * 30.0f;

    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f)   pct = 0.0f;

    return (uint8_t) pct;
}

bool isVbusConnected() {
    return digitalRead(VBUS_PIN) == HIGH;
}