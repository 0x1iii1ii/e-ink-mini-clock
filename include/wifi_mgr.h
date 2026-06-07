//
// Created by Li on 5/1/2026.
//

#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include "time_sync.h"
#include "display.h"
#include "config.h"

#define WIFI_CHECK_INTERVAL   30000UL   // check wifi every 30s
#define WIFI_RETRY_MAX        5         // give up after 5 tries, wait longer
#define WIFI_RETRY_LONG       300000UL  // 5 min backoff after max retries
#define SENSOR_INTERVAL       10000UL
#define WIFI_CONNECT_TIMEOUT  10000UL   // 10s to connect

void init_fs();
void load_config();
void save_config();
bool wifi_init();
void factory_reset();
void erase_config();
void maintainWifi();

#endif //WIFI_MGR_H
