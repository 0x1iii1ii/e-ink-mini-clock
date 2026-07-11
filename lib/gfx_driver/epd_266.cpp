#include "epd_266.h"

// ================= PUBLIC API =================
void EPD_266::begin() {
    gpioInit();
    initPanel();
    clear();
}

void EPD_266::setRotation(epd_rotation_t r) {
    rotation = r;
}

void EPD_266::clear() {
    memset(bufferBW, 0xFF, sizeof(bufferBW));
    memset(bufferR, 0xFF, sizeof(bufferR));
}

void EPD_266::display() {
    writeRAM(bufferBW, bufferR);
    update();
}

void EPD_266::sleep() {
    writeCmd(0x10);
    writeData(0x01);
    delay(200);
}

void EPD_266::update() {
    writeCmd(0x22);
    writeData(0xF4);
    writeCmd(0x20);
    waitBusy();
}

// ===== DRAW API =====
void EPD_266::drawPixel(int x, int y, epd_layer_t layer, uint8_t color) {
    transform(x, y);
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;
    uint16_t xBytes = EPD_HEIGHT / 8;
    uint16_t idx = x * xBytes + y / 8;
    uint8_t bit = 7 - (y % 8);
    uint8_t* buf = (layer == LAYER_RED) ? bufferR : bufferBW;
    if (color == BLACK) buf[idx] &= ~(1 << bit);
    else                buf[idx] |= (1 << bit);
}

void EPD_266::drawChar(int x, int y, char c, epd_layer_t layer) {
    if (c < ' ' || c > '~') c = ' ';
    const uint8_t* glyph = asc2_0806[c - ' '];

    for (int col = 0; col < 6; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 8; row++) {
            drawPixel(x + col, y + row, layer,
                (bits & (0x80 >> row)) ? BLACK : WHITE);
        }
    }
}

void EPD_266::drawString(int x, int y, const char* s, epd_layer_t layer) {
    while (*s) {
        drawChar(x, y, *s++, layer);
        x += 7;
    }
}

void EPD_266::drawLine(int x0, int y0, int x1, int y1, epd_layer_t layer) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        drawPixel(x0, y0, layer, BLACK);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void EPD_266::drawRect(int x, int y, int w, int h, epd_layer_t layer) {
    drawLine(x, y, x + w - 1, y, layer);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, layer);
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, layer);
    drawLine(x, y, x, y + h - 1, layer);
}

// ===== ROTATION =====
void EPD_266::transform(int& x, int& y) {
    int t;
    switch (rotation) {
    case ROT_90:
        t = x;
        x = EPD_WIDTH - 1 - y;
        y = t;
        break;
    case ROT_180:
        x = EPD_WIDTH - 1 - x;
        y = EPD_HEIGHT - 1 - y;
        break;
    case ROT_270:
        t = x;
        x = y;
        y = EPD_HEIGHT - 1 - t;
        break;
    default:
        break;
    }
}

// ===== LOW LEVEL =====
void EPD_266::gpioInit() {
    pinMode(EPD_SCL, OUTPUT);
    pinMode(EPD_SDA, OUTPUT);
    pinMode(EPD_RES, OUTPUT);
    pinMode(EPD_DC, OUTPUT);
    pinMode(EPD_CS, OUTPUT);
    pinMode(EPD_BUSY, INPUT);

    digitalWrite(EPD_CS, HIGH);
    digitalWrite(EPD_SCL, HIGH);
}

void EPD_266::spiWrite(uint8_t d) {
    digitalWrite(EPD_CS, LOW);
    for (int i = 0;i < 8;i++) {
        digitalWrite(EPD_SCL, LOW);
        digitalWrite(EPD_SDA, (d & 0x80));
        digitalWrite(EPD_SCL, HIGH);
        d <<= 1;
    }
    digitalWrite(EPD_CS, HIGH);
}

void EPD_266::writeCmd(uint8_t c) {
    digitalWrite(EPD_DC, LOW);
    spiWrite(c);
}

void EPD_266::writeData(uint8_t d) {
    digitalWrite(EPD_DC, HIGH);
    spiWrite(d);
}

void EPD_266::waitBusy() {
    while (digitalRead(EPD_BUSY)) delay(1);
}

void EPD_266::reset() {
    digitalWrite(EPD_RES, LOW);
    delay(10);
    digitalWrite(EPD_RES, HIGH);
    delay(10);
    waitBusy();
}

void EPD_266::initPanel() {
    reset();

    writeCmd(0x12);
    waitBusy();

    writeCmd(0x01);
    writeData(0x27);
    writeData(0x01);
    writeData(0x00);

    writeCmd(0x11);
    writeData(0x03);

    writeCmd(0x44);
    writeData(0x00);
    writeData(0x12);

    writeCmd(0x45);
    writeData(0x00);
    writeData(0x00);
    writeData(0x27);
    writeData(0x01);

    writeCmd(0x3C);
    writeData(0x05);

    writeCmd(0x18);
    writeData(0x80);

    writeCmd(0x4E);
    writeData(0x00);

    writeCmd(0x4F);
    writeData(0x00);
    writeData(0x00);

    waitBusy();
}

void EPD_266::writeRAM(uint8_t* bw, uint8_t* r) {
    uint16_t xBytes = EPD_HEIGHT / 8;
    uint16_t yLines = EPD_WIDTH;

    writeCmd(0x24);
    for (int y = 0;y < yLines;y++)
        for (int x = 0;x < xBytes;x++)
            writeData(bw[y * xBytes + x]);

    writeCmd(0x26);
    for (int y = 0;y < yLines;y++)
        for (int x = 0;x < xBytes;x++)
            writeData(~r[y * xBytes + x]);
}