//
// Created by Li on 3/12/2026.
//

#pragma once

#include <esp_sleep.h>
#include "config.h"
#include "wifi_mgr.h"
#include "web.h"
#include "display.h"

void enterPortalMode(bool factory = false);
void goToDeepSleep();
