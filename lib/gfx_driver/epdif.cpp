/**
 * epdif.cpp  —  EPD hardware interface implementation for ESP8266
 */

#include "epdif.h"
#include <SPI.h>

EpdIf::EpdIf()  {}
EpdIf::~EpdIf() {}

void EpdIf::DigitalWrite(int pin, int value) {
    digitalWrite(pin, value);
}

int EpdIf::DigitalRead(int pin) {
    return digitalRead(pin);
}

void EpdIf::DelayMs(unsigned int delaytime) {
    delay(delaytime);
}

void EpdIf::SpiTransfer(unsigned char data) {
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(data);
    digitalWrite(CS_PIN, HIGH);
}

int EpdIf::IfInit(void) {
    // pinMode(PWR_PIN,  OUTPUT);
    pinMode(CS_PIN,   OUTPUT);
    pinMode(RST_PIN,  OUTPUT);
    pinMode(DC_PIN,   OUTPUT);
    pinMode(BUSY_PIN, INPUT);

    // Power on the display boost circuit — WITHOUT this the display shows nothing
    // DigitalWrite(PWR_PIN, HIGH);
    delay(10);

#if defined(ESP32)
    SPI.begin(SPI_SCK, -1, SPI_MOSI, CS_PIN);
#else
    SPI.begin();
#endif

    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    return 0;
}
