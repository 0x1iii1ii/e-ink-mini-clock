//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "config.h"

String g_logBuf = "";
bool   g_serialEnabled = true;

void webLog(const String& msg) {
    Serial.println(msg);           // still prints to USB serial as normal
    if (!g_serialEnabled) return;
    g_logBuf += msg + "\n";
    if (g_logBuf.length() > LOG_BUF_SIZE)
        g_logBuf = g_logBuf.substring(g_logBuf.length() - LOG_BUF_SIZE);
}

void webLogf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    webLog(String(buf));
}