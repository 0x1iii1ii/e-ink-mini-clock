/**
 * epd2in66g.h  —  Waveshare 2.66" e-Paper (G) driver
 * Resolution: 360 x 184,  4-colour: Black / White / Yellow / Red
 */

#ifndef EPD2IN66G_H
#define EPD2IN66G_H

#include "epdif.h"

 // ── Panel resolution ──────────────────────────────────────
#define EPD_WIDTH    184
#define EPD_HEIGHT   360

// ── Colour constants (2-bit values packed 4-per-byte) ─────
#define EPD_BLACK    0x0
#define EPD_WHITE    0x1
#define EPD_YELLOW   0x2
#define EPD_RED      0x3

typedef unsigned int   UWORD;
typedef unsigned char  UBYTE;
typedef unsigned long  UDOUBLE;

// ── Frame buffer size ─────────────────────────────────────
// 2 bits per pixel → 8 pixels per 2 bytes → WIDTH*HEIGHT/4 bytes
#define EPD_BUFFER_SIZE  ((EPD_WIDTH * EPD_HEIGHT) / 4)

class Epd : EpdIf {
public:
    unsigned long WIDTH;
    unsigned long HEIGHT;

    Epd();
    ~Epd();

    int  Init();
    void Reset(void);
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void ReadBusyH(void);   // wait while BUSY = LOW  (display working)
    void ReadBusyL(void);   // wait while BUSY = HIGH (alternate polarity)
    void TurnOnDisplay(void);

    void Clear(UBYTE color);
    void Display(UBYTE* image);
    void Sleep(void);

    // Frame buffer helpers
    void     BufferClear(UBYTE* buf, UBYTE color);
    void     DrawPixel(UBYTE* buf, int x, int y, UBYTE color);
    UBYTE    GetPixel(UBYTE* buf, int x, int y);

private:
    unsigned int reset_pin;
    unsigned int dc_pin;
    unsigned int cs_pin;
    unsigned int busy_pin;
};

#endif
