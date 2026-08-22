#pragma once
#include <Arduino.h>
#include "global.h"

#define CONFIG_FILE "/config.json"
#define CONFIG_FS_VERSION 1

void init_fs();
void save_config();
void factory_reset();
void erase_config();
void format_fs();
void create_default_config();
void load_default_config(config_t& cfg);
void load_user_config(config_t& cfg);
void build_config_json(JsonDocument& doc);