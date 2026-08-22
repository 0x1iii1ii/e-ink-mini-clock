//
// Created by Li on 5/1/2026.
//

#ifndef WEB_H
#define WEB_H

#include "wifi_mgr.h"
#include "fs_config.h"
#include "time_sync.h"
#include "display.h"
#include "WiFiClientSecure.h"
#include <HTTPUpdate.h>

void handleRoot();
void web_loop();
void web_init();
void startWiFiPortal();

#endif //WEBSERVER_H
