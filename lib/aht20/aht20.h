//
// Created by Li on 5/1/2026.
//
#pragma once
#include <Wire.h>

#define AHT20_ADDR      0x38
#define AHT20_CMD_RESET 0xBA
#define AHT20_CMD_INIT  0xBE
#define AHT20_CMD_MEAS  0xAC

class AHT20 {
public:
    float temperature = 0;
    float humidity = 0;
    bool read();
    bool begin();
private:
    uint8_t getStatus();
};