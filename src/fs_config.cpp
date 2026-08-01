#include "fs_config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>   // v7

config_t cfg;

static const char* styleToStr(uint8_t s) {
    switch (s) {
    case CS_KHMER: return "khmer";
    case CS_RETRO: return "retro";
    default:       return "default";
    }
}
static uint8_t strToStyle(const char* s) {
    if (!strcmp(s, "khmer")) return CS_KHMER;
    if (!strcmp(s, "retro")) return CS_RETRO;
    return CS_DEFAULT;
}

static const char DEFAULT_CONFIG_JSON[] PROGMEM = R"json(
{
    "fsVersion": 1,
    "wifi": {
        "hostname": "eink",
        "networks": [
            { "ssid": "", "password": "" },
            { "ssid": "", "password": "" }
        ]
    },
    "clock": {
        "hour12": true,
        "utcOffset": 7,
        "refreshMin": 2,
        "ntpSyncDays": 7,
        "ntpReSyncDays": 1,
        "quietStart": 0,
        "quietEnd": 5,
        "quietEnabled": false,
        "powerSave": false
    },
    "display": {
        "style": "khmer",
        "showTemp": true,
        "showHum": true,
        "showBattPct": false,
        "showRssi": true
    },
    "alarms": [
        {
            "name": "Work",
            "hour": 7,
            "minute": 0,
            "enabled": true,
            "snooze": { "minutes": 5, "repeat": 2, "sound": 1 }
        }
    ]
}
)json";

void create_default_config() {
    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        Serial0.println("Failed to create config file");
        return;
    }
    f.print(DEFAULT_CONFIG_JSON);
    f.close();
}

// Populates cfg with hardcoded defaults. Runs BEFORE load_user_config so
// any field missing from the JSON on disk still ends up sane.
void load_default_config(config_t& cfg) {
    memset(&cfg, 0, sizeof(cfg));

    strlcpy(cfg.wifi.hostname, "eink", sizeof(cfg.wifi.hostname));
    strlcpy(cfg.wifi.wifi[0].ssid, "", sizeof(cfg.wifi.wifi[0].ssid));
    strlcpy(cfg.wifi.wifi[0].password, "", sizeof(cfg.wifi.wifi[0].password));
    strlcpy(cfg.wifi.wifi[1].ssid, "", sizeof(cfg.wifi.wifi[1].ssid));
    strlcpy(cfg.wifi.wifi[1].password, "", sizeof(cfg.wifi.wifi[1].password));

    cfg.clock.hour12 = true;
    cfg.clock.utcOffset = 7;
    cfg.clock.refreshMin = 2;
    cfg.clock.ntpSyncDays = 7;
    cfg.clock.ntpReSyncDays = 1;
    cfg.clock.quietStart = 0;
    cfg.clock.quietEnd = 5;
    cfg.clock.quietEnabled = false;
    cfg.clock.powerSave = false;

    cfg.display.showTemp = true;
    cfg.display.showHum = true;
    cfg.display.showBattPct = false;
    cfg.display.showRssi = true;
    cfg.display.clockStyle = CS_KHMER;

    alarm_t& a = cfg.clock.alarm[0];
    strlcpy(a.name, "Work", sizeof(a.name));
    a.hour = 7;
    a.minute = 0;
    a.enabled = false;
    a.repeatDays = 0;
    a.snooze.minutes = 5;
    a.snooze.repeat = 2;
    a.snooze.sound = 1;
    // alarm[1..4] stay zeroed (disabled)
}

// Overlays whatever's actually in CONFIG_FILE on top of cfg. Any field
// missing/malformed in the JSON just leaves the default from above in place.
void load_user_config(config_t& cfg) {
    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        Serial0.println("Failed to open config file, using defaults");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial0.printf("Config parse failed: %s, using defaults\n", err.c_str());
        return;
    }
    Serial0.println("Parsed JSON:");
    serializeJsonPretty(doc, Serial);
    Serial0.println();

    if (doc["wifi"]["hostname"].is<const char*>())
        strlcpy(cfg.wifi.hostname, doc["wifi"]["hostname"], sizeof(cfg.wifi.hostname));

    JsonArray networks = doc["wifi"]["networks"];
    if (!networks.isNull()) {
        uint8_t i = 0;
        for (JsonObject net : networks) {
            if (i >= 2) break;
            if (net["ssid"].is<const char*>())
                strlcpy(cfg.wifi.wifi[i].ssid, net["ssid"], sizeof(cfg.wifi.wifi[i].ssid));
            if (net["password"].is<const char*>())
                strlcpy(cfg.wifi.wifi[i].password, net["password"], sizeof(cfg.wifi.wifi[i].password));
            i++;
        }
    }

    JsonObject clk = doc["clock"];
    if (!clk.isNull()) {
        cfg.clock.hour12 = clk["hour12"] | cfg.clock.hour12;
        cfg.clock.utcOffset = clk["utcOffset"] | cfg.clock.utcOffset;
        cfg.clock.refreshMin = clk["refreshMin"] | cfg.clock.refreshMin;
        cfg.clock.ntpSyncDays = clk["ntpSyncDays"] | cfg.clock.ntpSyncDays;
        cfg.clock.ntpReSyncDays = clk["ntpReSyncDays"] | cfg.clock.ntpReSyncDays;
        cfg.clock.quietStart = clk["quietStart"] | cfg.clock.quietStart;
        cfg.clock.quietEnd = clk["quietEnd"] | cfg.clock.quietEnd;
        cfg.clock.quietEnabled = clk["quietEnabled"] | cfg.clock.quietEnabled;
        cfg.clock.powerSave = clk["powerSave"] | cfg.clock.powerSave;
    }

    JsonObject disp = doc["display"];
    if (!disp.isNull()) {
        cfg.display.showTemp = disp["showTemp"] | cfg.display.showTemp;
        cfg.display.showHum = disp["showHum"] | cfg.display.showHum;
        cfg.display.showBattPct = disp["showBattPct"] | cfg.display.showBattPct;
        cfg.display.showRssi = disp["showRssi"] | cfg.display.showRssi;
        if (disp["style"].is<const char*>())
            cfg.display.clockStyle = strToStyle(disp["style"]);
    }

    JsonArray alarms = doc["alarms"];
    if (!alarms.isNull()) {
        uint8_t i = 0;
        for (JsonObject a : alarms) {
            if (i >= 5) break;
            alarm_t& al = cfg.clock.alarm[i];
            if (a["name"].is<const char*>())
                strlcpy(al.name, a["name"], sizeof(al.name));
            al.hour = a["hour"] | al.hour;
            al.minute = a["minute"] | al.minute;
            al.enabled = a["enabled"] | al.enabled;
            al.repeatDays = a["repeatDays"] | al.repeatDays;

            JsonObject sn = a["snooze"];
            if (!sn.isNull()) {
                al.snooze.minutes = sn["minutes"] | al.snooze.minutes;
                al.snooze.repeat = sn["repeat"] | al.snooze.repeat;
                al.snooze.sound = sn["sound"] | al.snooze.sound;
            }
            i++;
        }
    }
    Serial0.println("=== load config done! ===");
}

void build_config_json(JsonDocument& doc) {
    doc["fsVersion"] = CONFIG_FS_VERSION;

    doc["wifi"]["hostname"] = cfg.wifi.hostname;
    JsonArray networks = doc["wifi"]["networks"].to<JsonArray>();
    for (uint8_t i = 0; i < 2; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = cfg.wifi.wifi[i].ssid;
        net["password"] = cfg.wifi.wifi[i].password;
    }

    JsonObject clk = doc["clock"].to<JsonObject>();
    clk["hour12"] = cfg.clock.hour12;
    clk["utcOffset"] = cfg.clock.utcOffset;
    clk["refreshMin"] = cfg.clock.refreshMin;
    clk["ntpSyncDays"] = cfg.clock.ntpSyncDays;
    clk["ntpReSyncDays"] = cfg.clock.ntpReSyncDays;
    clk["quietStart"] = cfg.clock.quietStart;
    clk["quietEnd"] = cfg.clock.quietEnd;
    clk["quietEnabled"] = cfg.clock.quietEnabled;
    clk["powerSave"] = cfg.clock.powerSave;

    JsonObject disp = doc["display"].to<JsonObject>();
    disp["style"] = styleToStr(cfg.display.clockStyle);
    disp["showTemp"] = cfg.display.showTemp;
    disp["showHum"] = cfg.display.showHum;
    disp["showBattPct"] = cfg.display.showBattPct;
    disp["showRssi"] = cfg.display.showRssi;

    JsonArray alarms = doc["alarms"].to<JsonArray>();
    for (uint8_t i = 0; i < 5; i++) {
        alarm_t& al = cfg.clock.alarm[i];
        JsonObject a = alarms.add<JsonObject>();
        a["name"] = al.name;
        a["hour"] = al.hour;
        a["minute"] = al.minute;
        a["enabled"] = al.enabled;
        a["repeatDays"] = al.repeatDays;
        JsonObject sn = a["snooze"].to<JsonObject>();
        sn["minutes"] = al.snooze.minutes;
        sn["repeat"] = al.snooze.repeat;
        sn["sound"] = al.snooze.sound;
    }
}

void save_config() {
    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        Serial0.println("Failed to open config file for writing");
        return;
    }

    JsonDocument doc;
    build_config_json(doc);

    Serial0.println("json config updated:");
    serializeJsonPretty(doc, Serial0);

    serializeJson(doc, f);
    f.close();
}

void factory_reset() {
    LittleFS.end();

    if (!LittleFS.format()) {
        Serial0.println("Format failed");
    }
    else {
        Serial0.println("Format OK");
    }

    LittleFS.begin(true);
}

void erase_config() {
    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
        Serial0.println("Config erased");
    }
}

void init_fs() {
    if (!LittleFS.begin()) {
        Serial0.println("LittleFS mount failed, formatting");
        LittleFS.format();
        LittleFS.begin();
    }

    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial0.println("Config file not found, creating default config");
        create_default_config();
    }

    load_default_config(cfg);
    load_user_config(cfg);
}