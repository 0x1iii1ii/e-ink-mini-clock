#include "fs_config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>   // v7

config_t cfg;

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
        "ntpSyncDays": 7,
        "ntpReSyncDays": 1
    },
    "display": {
        "refreshMin": 2,
        "style": "default",
        "showTemp": true,
        "showHum": true,
        "showBattPct": false,
        "showRssi": true
    },
    "device": {
        "quietStart": 0,
        "quietEnd": 5,
        "quietEnabled": false,
        "powerSave": false
    },
    "alarms": []
}
)json";

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

static bool copyString(JsonObject src, const char* key, JsonObject dst) {
    if (src[key].is<const char*>()) {
        dst[key] = src[key];
        return true;
    }
    return false;
}

static bool migrate_v0_to_v1(JsonDocument& oldDoc, JsonDocument& newDoc) {
    JsonObject old = oldDoc.as<JsonObject>();
    JsonObject root = newDoc.as<JsonObject>();
    // ---------------------------------------------------------
    // wifi
    // ---------------------------------------------------------
    JsonObject wifi = root["wifi"];
    JsonArray networks = wifi["networks"];
    if (old["ssid"].is<const char*>())
        networks[0]["ssid"] = old["ssid"];
    if (old["password"].is<const char*>())
        networks[0]["password"] = old["password"];
    if (old["hostname"].is<const char*>())
        wifi["hostname"] = old["hostname"];
    // ---------------------------------------------------------
    // clock
    // ---------------------------------------------------------
    JsonObject clock = root["clock"];
    if (!old["hour12"].isNull())
        clock["hour12"] = old["hour12"];
    if (!old["utcOffset"].isNull())
        clock["utcOffset"] = old["utcOffset"];
    if (!old["ntpSyncDays"].isNull())
        clock["ntpSyncDays"] = old["ntpSyncDays"];
    if (!old["ntpReSyncDays"].isNull())
        clock["ntpReSyncDays"] = old["ntpReSyncDays"];
    // ---------------------------------------------------------
    // display
    // ---------------------------------------------------------
    JsonObject display = root["display"];
    if (!old["refreshMin"].isNull())
        display["refreshMin"] = old["refreshMin"];
    if (!old["showTemp"].isNull())
        display["showTemp"] = old["showTemp"];
    if (!old["showHum"].isNull())
        display["showHum"] = old["showHum"];
    if (!old["showBattPct"].isNull())
        display["showBattPct"] = old["showBattPct"];
    if (!old["showRSSI"].isNull())
        display["showRssi"] = old["showRSSI"];
    // ---------------------------------------------------------
    // device
    // ---------------------------------------------------------
    JsonObject device = root["device"];
    if (!old["quietStart"].isNull())
        device["quietStart"] = old["quietStart"];
    if (!old["quietEnd"].isNull())
        device["quietEnd"] = old["quietEnd"];
    if (!old["quietEnabled"].isNull())
        device["quietEnabled"] = old["quietEnabled"];
    if (!old["powerSave"].isNull())
        device["powerSave"] = old["powerSave"];
    // ---------------------------------------------------------
    // Set new version
    // ---------------------------------------------------------
    root["fsVersion"] = CONFIG_FS_VERSION;
    return true;
}

static bool migrate_v1_to_v2(
    JsonDocument& oldDoc,
    JsonDocument& newDoc) {
    JsonObject old = oldDoc.as<JsonObject>();
    JsonObject root = newDoc.as<JsonObject>();

    // Only code for things that changed in v2.

    // Example:
    // v2 added display.brightness.
    //
    // If v1 had no brightness, leave the value from
    // DEFAULT_CONFIG_JSON untouched.

    // If v2 renamed something:
    // newDoc["display"]["newName"] =
    //     oldDoc["display"]["oldName"];

    root["fsVersion"] = 2;

    return true;
}

static bool migrate_config_file() {
    File f = LittleFS.open(CONFIG_FILE, "r");

    if (!f) {
        Serial0.println("Failed to open config");
        return false;
    }

    JsonDocument oldDoc;
    DeserializationError err = deserializeJson(oldDoc, f);
    f.close();

    if (err) {
        Serial0.printf("Config parse failed: %s\n", err.c_str());
        return false;
    }
    // check fs format version
    JsonVariant version = oldDoc["fsVersion"];
    uint32_t oldVersion = 0;
    if (version.isNull()) {
        // Old format has no fsVersion.Therefore it is version 0.
        oldVersion = 0;
        Serial0.println("No fsVersion found, assuming config version 0");
    }
    else {
        oldVersion = version | 0;
        Serial0.printf("Config version: %lu\n", (unsigned long) oldVersion);
    }
    if (oldVersion == CONFIG_FS_VERSION) {
        Serial0.println("Config already at current version");
        return true;
    }
    JsonDocument newDoc;
    err = deserializeJson(newDoc, DEFAULT_CONFIG_JSON);
    if (err) {
        Serial0.printf("Failed to parse default config: %s\n", err.c_str());
        return false;
    }
    // Migrate old configuration
    while (oldVersion < CONFIG_FS_VERSION) {
        bool success = false;
        switch (oldVersion) {
        case 0:
            success = migrate_v0_to_v1(oldDoc, newDoc);
            break;
        case 1:
            // success = migrate_v1_to_v2(oldDoc, newDoc);
            break;
        default:
            Serial0.printf(
                "Unsupported config version: %lu\n",
                (unsigned long) oldVersion
            );
            return false;
        }
        if (!success)
            return false;
        oldVersion++;
    }

    //Write NEW configuration
    File out = LittleFS.open(CONFIG_FILE, "w");
    if (!out) {
        Serial0.println("Failed to open config for writing");
        return false;
    }

    // serializeJsonPretty(newDoc, Serial0);
    Serial0.println();
    size_t written = serializeJson(newDoc, out);
    out.close();

    if (written == 0) {
        Serial0.println("Failed to write migrated config");
        return false;
    }
    Serial0.println("Config migration successful");
    return true;
}

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
    cfg.clock.ntpSyncDays = 7;
    cfg.clock.ntpReSyncDays = 1;

    cfg.device.quietStart = 0;
    cfg.device.quietEnd = 5;
    cfg.device.quietEnabled = false;
    cfg.device.powerSave = false;

    cfg.display.refreshMin = 3;
    cfg.display.showTemp = true;
    cfg.display.showHum = true;
    cfg.display.showBattPct = false;
    cfg.display.showRssi = true;
    cfg.display.clockStyle = CS_DEFAULT;

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
    // Serial0.println("Parsed JSON:");
    // serializeJsonPretty(doc, Serial);
    // Serial0.println();
    Serial0.println("Json config version: " + String(doc["fsVersion"] | 0));

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
        cfg.clock.ntpSyncDays = clk["ntpSyncDays"] | cfg.clock.ntpSyncDays;
        cfg.clock.ntpReSyncDays = clk["ntpReSyncDays"] | cfg.clock.ntpReSyncDays;

    }

    JsonObject disp = doc["display"];
    if (!disp.isNull()) {
        cfg.display.refreshMin = disp["refreshMin"] | cfg.display.refreshMin;
        cfg.display.showTemp = disp["showTemp"] | cfg.display.showTemp;
        cfg.display.showHum = disp["showHum"] | cfg.display.showHum;
        cfg.display.showBattPct = disp["showBattPct"] | cfg.display.showBattPct;
        cfg.display.showRssi = disp["showRssi"] | cfg.display.showRssi;
        if (disp["style"].is<const char*>())
            cfg.display.clockStyle = strToStyle(disp["style"]);
    }

    JsonObject dev = doc["device"];
    if (!dev.isNull()) {
        cfg.device.quietStart = dev["quietStart"] | cfg.device.quietStart;
        cfg.device.quietEnd = dev["quietEnd"] | cfg.device.quietEnd;
        cfg.device.quietEnabled = dev["quietEnabled"] | cfg.device.quietEnabled;
        cfg.device.powerSave = dev["powerSave"] | cfg.device.powerSave;
    }

    JsonArray alarms = doc["alarms"];
    if (!alarms.isNull()) {
        uint8_t i = 0;
        for (JsonObject a : alarms) {
            if (i >= MAX_ALARMS) break;
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
    clk["ntpSyncDays"] = cfg.clock.ntpSyncDays;
    clk["ntpReSyncDays"] = cfg.clock.ntpReSyncDays;


    JsonObject disp = doc["display"].to<JsonObject>();
    disp["refreshMin"] = cfg.display.refreshMin;
    disp["style"] = styleToStr(cfg.display.clockStyle);
    disp["showTemp"] = cfg.display.showTemp;
    disp["showHum"] = cfg.display.showHum;
    disp["showBattPct"] = cfg.display.showBattPct;
    disp["showRssi"] = cfg.display.showRssi;

    JsonObject dev = doc["device"].to<JsonObject>();
    dev["quietStart"] = cfg.device.quietStart;
    dev["quietEnd"] = cfg.device.quietEnd;
    dev["quietEnabled"] = cfg.device.quietEnabled;
    dev["powerSave"] = cfg.device.powerSave;

    JsonArray alarms = doc["alarms"].to<JsonArray>();
    for (uint8_t i = 0; i < MAX_ALARMS; i++) {
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

    Serial0.println("json config updated!");
    // serializeJsonPretty(doc, Serial0);

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
        Serial0.println("Config erased!");
    }
    LittleFS.format();
    Serial0.println("FS formatted!");
}

void format_fs() {
    if (LittleFS.format())
        Serial0.println("FS formatted!");
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
    else migrate_config_file();

    load_default_config(cfg);
    load_user_config(cfg);
}