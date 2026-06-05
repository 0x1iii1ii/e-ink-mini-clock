//
// Created by Li on 3/12/2026.
//

#ifndef BATTERY_H
#define BATTERY_H

#include "Arduino.h"
#include "config.h"

// Returns battery percentage 0-100, assumes 3.0V=0% 4.2V=100%
uint8_t readBattery(void);
void battery_init();

// Returns true if USB power connected
bool isVbusConnected(void);

#endif //BATTERY_H
