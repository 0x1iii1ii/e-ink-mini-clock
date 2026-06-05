//
// Created by Li on 5/1/2026.
//

#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include "time_sync.h"
#include "display.h"
#include "config.h"

void init_fs();
void load_config();
void save_config();
void wifi_init();
void factory_reset();
void erase_config();

#endif //WIFI_MGR_H
