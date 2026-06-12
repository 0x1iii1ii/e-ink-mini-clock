//
// Created by Li on 5/1/2026.
//

#ifndef WEB_H
#define WEB_H

#include "wifi_mgr.h"
#include "time_sync.h"
#include "display.h"
#include "WiFiClientSecure.h"
#include <HTTPUpdate.h>

extern unsigned long lastHttpActivityMs;
bool isWebClientActive();
String htmlHead(const char* title = "e-Ink Clock");
void handleRoot();
void handleSave();
void handleRefresh();
void handleOTA();
void web_loop();
void web_init();
// Add declaration for WiFi configuration portal
void startWiFiPortal();

#endif //WEBSERVER_H
