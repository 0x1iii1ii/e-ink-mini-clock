#pragma once
#include <Arduino.h>

#include "epd_fonts.h"

// ================= CONFIG =================
#define EPD_SCL 6
#define EPD_SDA 7
#define EPD_RES 8
#define EPD_DC  19
#define EPD_CS  2
#define EPD_BUSY 5

#define EPD_WIDTH  296
#define EPD_HEIGHT 152

#define WHITE 1
#define BLACK 0

// ================= FONT =================
extern const unsigned char asc2_0806[][6];

typedef enum {
    LAYER_BW = 0,
    LAYER_RED = 1
} epd_layer_t;

typedef enum {
    ROT_0 = 0,
    ROT_90,
    ROT_180,
    ROT_270
} epd_rotation_t;

// ================= CLASS =================
class EPD_266 {
public:
    uint8_t bufferBW[5624];
    uint8_t bufferR[5624];

    epd_rotation_t rotation = ROT_0;

    void begin();
    void setRotation(epd_rotation_t r);
    void clear();

    // ===== REFRESH MODES =====
    void display();                    // Non-blocking: write RAM + start refresh
    void sleep();
    void update();                     // Non-blocking: start refresh
    bool isBusy();                     // Non-blocking: check if display is busy
    void waitForRefresh();             // Blocking: wait for refresh to complete

    // ===== DRAW API =====
    void drawPixel(int x, int y, epd_layer_t layer, uint8_t color);
    void drawChar(int x, int y, char c, epd_layer_t layer);
    void drawString(int x, int y, const char* s, epd_layer_t layer);
    void drawLine(int x0, int y0, int x1, int y1, epd_layer_t layer);
    void drawRect(int x, int y, int w, int h, epd_layer_t layer);

private:
    // ===== ROTATION =====
    void transform(int& x, int& y);

    // ===== LOW LEVEL =====
    void gpioInit();
    void spiWrite(uint8_t d);
    void writeCmd(uint8_t c);
    void writeData(uint8_t d);
    bool checkBusy();                  // Non-blocking: just reads BUSY pin
    void waitBusy();                   // Blocking: waits for BUSY to go LOW
    void reset();
    void initPanel();
    void writeRAM(uint8_t* bw, uint8_t* r);
};