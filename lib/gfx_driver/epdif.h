/**
 * epdif.h  —  EPD hardware interface for ESP8266
 *
 */

#ifndef EPDIF_H
#define EPDIF_H

#include <Arduino.h>

 // ── Pin definitions for ESP32-C3 ──
 // #define PWR_PIN   3    // any free GPIO
#define RST_PIN   4
#define DC_PIN    1
#define CS_PIN    2 //was 10
#define BUSY_PIN  5
#define SPI_SCK   6
#define SPI_MOSI  7

class EpdIf {
public:
    EpdIf(void);
    ~EpdIf(void);
    static int  IfInit(void);
    static void DigitalWrite(int pin, int value);
    static int  DigitalRead(int pin);
    static void DelayMs(unsigned int delaytime);
    static void SpiTransfer(unsigned char data);
};

#endif
