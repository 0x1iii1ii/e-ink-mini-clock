/**
 * epd2in66g.cpp  —  Waveshare 2.66" e-Paper (G) driver
 * Ported from official Waveshare Arduino library, adapted for ESP8266
 */

#include <stdlib.h>
#include "epd2in66g.h"

Epd::~Epd() {}

Epd::Epd() {
    reset_pin = RST_PIN;
    dc_pin    = DC_PIN;
    cs_pin    = CS_PIN;
    busy_pin  = BUSY_PIN;
    WIDTH     = EPD_WIDTH;
    HEIGHT    = EPD_HEIGHT;
}

int Epd::Init() {
    if (IfInit() != 0) return -1;

    Reset();
    ReadBusyH();

    SendCommand(0x4D);
    SendData(0x78);

    SendCommand(0x00);  // PSR
    SendData(0x0F);
    SendData(0x29);

    SendCommand(0x01);  // PWRR
    SendData(0x07);
    SendData(0x00);

    SendCommand(0x03);  // POFS
    SendData(0x10);
    SendData(0x54);
    SendData(0x44);

    SendCommand(0x06);  // BTST_P
    SendData(0x05);
    SendData(0x00);
    SendData(0x3F);
    SendData(0x0A);
    SendData(0x25);
    SendData(0x12);
    SendData(0x1A);

    SendCommand(0x50);  // CDI
    SendData(0x37);

    SendCommand(0x60);  // TCON
    SendData(0x02);
    SendData(0x02);

    SendCommand(0x61);  // TRES — resolution
    SendData(WIDTH  / 256);
    SendData(WIDTH  % 256);
    SendData(HEIGHT / 256);
    SendData(HEIGHT % 256);

    SendCommand(0xE7);
    SendData(0x1C);

    SendCommand(0xE3);
    SendData(0x22);

    SendCommand(0xB4);
    SendData(0xD0);
    SendCommand(0xB5);
    SendData(0x03);

    SendCommand(0xE9);
    SendData(0x01);

    SendCommand(0x30);
    SendData(0x08);

    SendCommand(0x04);  // Power ON
    ReadBusyH();
    return 0;
}

void Epd::SendCommand(unsigned char command) {
    DigitalWrite(dc_pin, LOW);
    SpiTransfer(command);
}

void Epd::SendData(unsigned char data) {
    DigitalWrite(dc_pin, HIGH);
    SpiTransfer(data);
}

void Epd::ReadBusyH(void) {
    while (DigitalRead(busy_pin) == LOW) {
        DelayMs(5);
    }
}

void Epd::ReadBusyL(void) {
    while (DigitalRead(busy_pin) == HIGH) {
        DelayMs(5);
    }
}

void Epd::Reset(void) {
    DigitalWrite(reset_pin, HIGH);  DelayMs(20);
    DigitalWrite(reset_pin, LOW);   DelayMs(2);
    DigitalWrite(reset_pin, HIGH);  DelayMs(20);
}

void Epd::TurnOnDisplay(void) {
    SendCommand(0x12);  // DISPLAY_REFRESH
    SendData(0x00);
    ReadBusyH();
}

void Epd::Clear(UBYTE color) {
    UWORD w = (WIDTH % 4 == 0) ? (WIDTH / 4) : (WIDTH / 4 + 1);
    UBYTE fill = (color << 6) | (color << 4) | (color << 2) | color;
    SendCommand(0x10);
    for (UWORD j = 0; j < HEIGHT; j++)
        for (UWORD i = 0; i < w; i++)
            SendData(fill);
    TurnOnDisplay();
}

void Epd::Display(UBYTE *image) {
    UWORD w = (WIDTH % 4 == 0) ? (WIDTH / 4) : (WIDTH / 4 + 1);
    SendCommand(0x10);
    for (UWORD j = 0; j < HEIGHT; j++)
        for (UWORD i = 0; i < w; i++)
            SendData(pgm_read_byte(&image[i + j * w]));
    TurnOnDisplay();
}

void Epd::Sleep(void) {
    SendCommand(0x02);  // POWER_OFF
    SendData(0x00);
    ReadBusyH();
    SendCommand(0x07);  // DEEP_SLEEP
    SendData(0xA5);
}

// ════════════════════════════════════════════════════════════
//  Frame buffer helpers
//  Buffer layout: 2 bits/pixel, row-major, MSB first
//  Byte index = (y * stride) + (x / 4)
//  Bit shift  = 6 - (x % 4) * 2
// ════════════════════════════════════════════════════════════

// void Epd::BufferClear(UBYTE *buf, UBYTE color) {
//     UWORD stride = (WIDTH % 4 == 0) ? (WIDTH / 4) : (WIDTH / 4 + 1);
//     UBYTE fill   = (color << 6) | (color << 4) | (color << 2) | color;
//     for (UWORD i = 0; i < stride * HEIGHT; i++) buf[i] = fill;
// }

void Epd::BufferClear(UBYTE *buf, UBYTE color) {
    UBYTE fill = (color << 6) | (color << 4) | (color << 2) | color;
    for (UWORD i = 0; i < 46 * 360; i++) buf[i] = fill;
}

void Epd::DrawPixel(UBYTE *buf, int x, int y, UBYTE color) {
    // Rotate 90° CW: drawing space is 360 wide x 184 tall
    // Map (x,y) in landscape → (px,py) in native portrait 184x360
    int px = y;           // landscape y → portrait x  (0..183)
    int py = 360 - 1 - x; // landscape x → portrait y  (flip)
    if (px < 0 || px >= 184 || py < 0 || py >= 360) return;
    UWORD stride = (184 % 4 == 0) ? (184 / 4) : (184 / 4 + 1);  // = 46
    UWORD idx    = py * stride + (px / 4);
    int   shift  = 6 - (px % 4) * 2;
    buf[idx] = (buf[idx] & ~(0x03 << shift)) | ((color & 0x03) << shift);
}

// void Epd::DrawPixel(UBYTE *buf, int x, int y, UBYTE color) {
//     if (x < 0 || x >= (int)WIDTH || y < 0 || y >= (int)HEIGHT) return;
//     UWORD stride = (WIDTH % 4 == 0) ? (WIDTH / 4) : (WIDTH / 4 + 1);
//     UWORD idx    = y * stride + (x / 4);
//     int   shift  = 6 - (x % 4) * 2;
//     buf[idx] = (buf[idx] & ~(0x03 << shift)) | ((color & 0x03) << shift);
// }

UBYTE Epd::GetPixel(UBYTE *buf, int x, int y) {
    if (x < 0 || x >= (int)WIDTH || y < 0 || y >= (int)HEIGHT) return EPD_WHITE;
    UWORD stride = (WIDTH % 4 == 0) ? (WIDTH / 4) : (WIDTH / 4 + 1);
    UWORD idx    = y * stride + (x / 4);
    int   shift  = 6 - (x % 4) * 2;
    return (buf[idx] >> shift) & 0x03;
}
