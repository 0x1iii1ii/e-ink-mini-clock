//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "web.h"
#include "index_html.h"
#include "style_css.h"
#include "script_js.h"
#include "favicon_ico.h"

// ════════════════════════════════════════════════════════════
//  Web server
// ════════════════════════════════════════════════════════════

WebServer        server(80);
HTTPUpdateServer httpUpdater;

// Uptime tracking (seconds since boot)
time_t lastRefreshEpoch = 0;
static unsigned long _bootMs = 0;
static String   _updatePayload = "";
static bool     _updateChecking = false;
static bool     _updateReady = false;

// ── Static file helper ─────────────────────────────────────
struct GzResource {
  const char* path;
  const unsigned char* data;
  size_t length;
  const char* contentType;
};

static const GzResource gz_resources[] = {
  { "/", (const unsigned char*) index_html, sizeof(index_html), "text/html" },
  { "/index.html", (const unsigned char*) index_html, sizeof(index_html), "text/html" },
  { "/style.css", (const unsigned char*) style_css, sizeof(style_css), "text/css" },
  { "/script.js", (const unsigned char*) script_js, sizeof(script_js), "application/javascript" },
  { "/favicon.ico", (const unsigned char*) favicon_ico, sizeof(favicon_ico), "image/x-icon" },
};

bool handleFileRead(String path) {
  // Normalize root path
  if (path == "/") path = "/index.html";
  // Search for matching resource
  for (const auto& resource : gz_resources) {
    if (path == resource.path) {
      // Serve with gzip encoding header
      server.setContentLength(resource.length);
      server.sendHeader("Content-Encoding", "gzip");
      server.send(200, resource.contentType, "");
      server.sendContent_P((const char*) resource.data, resource.length);
      return true;
    }
  }
  Serial0.println("Resource not found: " + path);
  return false;
}

void handleNotFound() {
  if (!handleFileRead(server.uri()))
    server.send(404, "text/plain", "Not Found");
}

// ── CORS helper ────────────────────────────────────────────
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ── GET /config ────────────────────────────────────────────
// Returns full device config as JSON so the portal can
// pre-populate all fields on load.
void handleConfig() {
  addCORSHeaders();

  JsonDocument doc;
  build_config_json(doc);

  String json;
  serializeJson(doc, json);
  // serializeJsonPretty(doc, Serial0);

  server.send(200, "application/json", json);
}

// ── POST /ota-url ─────────────────────────────────────────
// Triggers OTA update from a given URL (called from the Update tab)
void handleOtaUrl() {
  addCORSHeaders();

  String url = server.arg("url");
  if (url.isEmpty()) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"No URL\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Starting OTA from URL...\"}");

  // Follow redirects manually
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  t_httpUpdate_return ret;

  if (url.startsWith("https://")) {
    Serial0.println("Using HTTPS OTA");
    WiFiClientSecure client;
    client.setInsecure();
    ret = httpUpdate.update(client, url);
  }
  else if (url.startsWith("http://")) {
    Serial0.println("Using HTTP OTA");
    WiFiClient client;
    ret = httpUpdate.update(client, url);
  }
  else {
    Serial0.println("Invalid URL scheme");
    return;
  }

  switch (ret) {
  case HTTP_UPDATE_OK:
    Serial0.println("OTA Success");
    ESP.restart();
    break;

  case HTTP_UPDATE_FAILED:
    Serial0.printf("OTA failed: %s\n",
      httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial0.println("No update available");
    break;
  }
}

void doVersionFetch() {
  if (!_updateChecking) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  https.setTimeout(8000);
  https.begin(client, "https://raw.githubusercontent.com/0x1iii1ii/e-ink-mini-clock/refs/heads/main/version.json");
  int code = https.GET();
  if (code == 200) {
    _updatePayload = https.getString();
    Serial0.println("Fetched version info: " + _updatePayload);
  }
  else {
    _updatePayload = "{\"ok\":false,\"msg\":\"Fetch failed\"}";
  }
  https.end();
  _updateChecking = false;
  _updateReady = true;
}

void handleCheckUpdate() {
  addCORSHeaders();

  if (_updateChecking) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"Already checking\"}");
    return;
  }
  if (_updateReady) {
    // Return cached result immediately
    _updateReady = false;
    server.send(200, "application/json", _updatePayload);
    return;
  }
  // Kick off in background via a flag — handled in loop()
  _updateChecking = true;
  server.send(200, "application/json", "{\"ok\":false,\"msg\":\"checking\",\"pending\":true}");
}

// ── GET /status ────────────────────────────────────────────
// Polled every few seconds by the portal to update the
// live header stats (time, RSSI, uptime, heap, NTP).
void handleStatus() {
  addCORSHeaders();

  struct tm t;
  if (!getRtcTime(&t)) {
    Serial0.println("RTC read failed");
    return;
  }

  time_t now;
  now = mktime(&t);

  char timeBuf[20];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%d:%d", t.tm_hour, t.tm_min, t.tm_sec);

  unsigned long uptimeSec = (millis() - _bootMs) / 1000UL;
  uint32_t d = uptimeSec / 86400;
  uint8_t  h = (uptimeSec % 86400) / 3600;
  uint8_t  mi = (uptimeSec % 3600) / 60;
  uint8_t  s = uptimeSec % 60;
  char uptimeBuf[24];
  snprintf(uptimeBuf, sizeof(uptimeBuf), "%ud %02u:%02u:%02u", d, h, mi, s);

  // Filesystem usage
  uint32_t fsUsedKB = LittleFS.usedBytes() / 1024;
  uint32_t fsTotalKB = LittleFS.totalBytes() / 1024;
  char fsBuf[24];
  snprintf(fsBuf, sizeof(fsBuf), "%u / %u KB", fsUsedKB, fsTotalKB);

  // lastRefreshEpoch = now;

  // Last refresh timestamp
  char lastRefBuf[20] = "Never";
  if (lastRefreshEpoch > 0)
    snprintf(lastRefBuf, sizeof(lastRefBuf), "%d:%d:%d", t.tm_hour, t.tm_min, t.tm_sec);

  String json = "{";
  json += "\"time\":\"" + String(timeBuf) + "\",";
  json += "\"uptime\":\"" + String(uptimeBuf) + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"heap\":" + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"flashMB\":" + String(ESP.getFlashChipSize() / 1048576) + ",";
  json += "\"fs\":\"" + String(fsBuf) + "\",";
  json += "\"cpu\":" + String(ESP.getCpuFreqMHz()) + ",";
  json += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
  json += "\"sdk\":\"" + String(ESP.getSdkVersion()) + "\",";
  json += "\"firmware\":\""  FW_VERSION "\",";
  json += "\"ntpSynced\":" + String(now > 1000000 ? "true" : "false") + ",";
  json += "\"lastRefresh\":\"" + String(lastRefBuf) + "\",";
  json += "\"temp\":" + String(g_temperature, 1) + ",";
  json += "\"hum\":" + String(g_humidity, 1) + ",";
  json += "\"batPct\":" + String(g_batteryPct) + ",";
  json += "\"charging\":" + String(g_isVbusConnected ? "true" : "false") + "";
  json += "}";
  server.send(200, "application/json", json);
}

// ── GET /scan ──────────────────────────────────────────────
// Returns visible WiFi networks; called by the portal's
// network list on load (and on manual rescan).
void handleScan() {
  addCORSHeaders();
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN
      ? "true" : "false");
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// ── POST /save-clock ───────────────────────────────────────
// Saves clock preferences from the Clock tab.
void handleSaveClock() {
  addCORSHeaders();

  // value inputs
  if (server.hasArg("utcOffset"))
    cfg.clock.utcOffset = (int8_t) server.arg("utcOffset").toInt();
  if (server.hasArg("ntpSyncDays"))
    cfg.clock.ntpSyncDays = (uint8_t) constrain(server.arg("ntpSyncDays").toInt(), 1, 7);
  if (server.hasArg("ntpReSyncDays"))
    cfg.clock.ntpReSyncDays = (uint8_t) constrain(server.arg("ntpReSyncDays").toInt(), 1, 3); // limit to 3 days 

  // Alarm settings
  if (server.hasArg("alarmHour"))
    cfg.clock.alarm[0].hour = (uint8_t) constrain(server.arg("alarmHour").toInt(), 0, 23);
  if (server.hasArg("alarmMinute"))
    cfg.clock.alarm[0].minute = (uint8_t) constrain(server.arg("alarmMinute").toInt(), 0, 59);

  // check boxes
  cfg.clock.hour12 = server.hasArg("hour12");
  cfg.clock.alarm[0].enabled = server.hasArg("alarmEnabled");

  save_config();
  server.send(200, "application/json", "{\"ok\":true}");

  delay(2000);
  ESP.restart();
}

// ── POST /save-display ───────────────────────────────────────
// Saves display preferences from the Display tab.
void handleSaveDisplay() {
  addCORSHeaders();

  // Refresh interval
  if (server.hasArg("refreshMin"))
    cfg.display.refreshMin = (uint8_t) constrain(server.arg("refreshMin").toInt(), 1, 60);

  // Clock style
  if (server.hasArg("clockStyle")) {
    String style = server.arg("clockStyle");

    if (style == "default") {
      cfg.display.clockStyle = CS_DEFAULT;
    }
    else if (style == "khmer") {
      cfg.display.clockStyle = CS_KHMER;
    }
    else if (style == "retro") {
      cfg.display.clockStyle = CS_RETRO;
    }
  }

  // Checkboxes
  cfg.display.showBattPct = server.hasArg("showBattPct");
  cfg.display.showHum = server.hasArg("showHum");
  cfg.display.showTemp = server.hasArg("showTemp");
  cfg.display.showRssi = server.hasArg("showRSSI");

  save_config();
  server.send(200, "application/json", "{\"ok\":true}");

  delay(2000);
  ESP.restart();
}

// ── POST /save-device ───────────────────────────────────────
// Saves device preferences from the Device tab.
void handleSaveDevice() {
  addCORSHeaders();

  // value inputs
  if (server.hasArg("quietStart"))
    cfg.device.quietStart = (uint8_t) constrain(server.arg("quietStart").toInt(), 0, 23);
  if (server.hasArg("quietEnd"))
    cfg.device.quietEnd = (uint8_t) constrain(server.arg("quietEnd").toInt(), 0, 23);

  // check boxes
  cfg.device.quietEnabled = server.hasArg("quietEnabled");
  cfg.device.powerSave = server.hasArg("powerSave");

  save_config();
  server.send(200, "application/json", "{\"ok\":true}");

  delay(2000);
  ESP.restart();
}

// ── POST /save-wifi ────────────────────────────────────────
// Saves WiFi credentials then reboots into STA mode.
void handleSaveWifi() {
  addCORSHeaders();

  if (server.hasArg("ssid"))
    strlcpy(cfg.wifi.wifi->ssid, server.arg("ssid").c_str(), sizeof cfg.wifi.wifi->ssid);
  if (server.hasArg("pass"))
    strlcpy(cfg.wifi.wifi->password, server.arg("pass").c_str(), sizeof cfg.wifi.wifi->password);

  save_config();
  server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Saved. Rebooting...\"}");
  delay(1200);
  ESP.restart();
}

// ── POST /save-hostname ────────────────────────────────────
void handleSaveHostname() {
  addCORSHeaders();
  if (server.hasArg("hostnameVal"))
    strlcpy(cfg.wifi.hostname, server.arg("hostnameVal").c_str(), sizeof(cfg.wifi.hostname));
  save_config();
  server.send(200, "application/json", "{\"ok\":true}");
}

// ── POST /save-alarms ──────────────────────────────────────
void handleSaveAlarms() {
  addCORSHeaders();

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"No body\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Bad JSON\"}");
    return;
  }

  JsonArray alarms = doc["alarms"];
  if (alarms.isNull()) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"No alarms field\"}");
    return;
  }

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
  // Disable any leftover slots not sent
  for (; i < 5; i++) cfg.clock.alarm[i].enabled = false;

  save_config();
  server.send(200, "application/json", "{\"ok\":true}");
}

// ── POST /action ───────────────────────────────────────────
// Single endpoint for all quick-action buttons.
// ?cmd = refresh | sync | clear | restart | factory
void handleAction() {
  addCORSHeaders();

  String cmd = server.arg("cmd");

  if (cmd == "refresh") {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Refreshing display...\"}");
    drawDisplay();

  }
  else if (cmd == "sync") {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"NTP sync triggered.\"}");
    sync_time();

  }
  else if (cmd == "clear") {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Display cleared.\"}");
    drawDisplay();

  }
  else if (cmd == "restart") {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Restarting...\"}");
    delay(500);
    ESP.restart();

  }
  else if (cmd == "factory") {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"User reset. Restarting...\"}");
    erase_config();
    delay(500);
    ESP.restart();
  }
  else if (cmd == "user_factory") {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Factory reset. Restarting...\"}");
    format_fs();
    delay(500);
    ESP.restart();
  }
  else {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Unknown command.\"}");
  }
}

// ── Root ───────────────────────────────────────────────────
void handleRoot() {
  if (!handleFileRead("/"))
    server.send(404, "text/plain", "Portal not found — upload /web/index.html via LittleFS");
}

// ── web_init ───────────────────────────────────────────────
void web_init() {
  _bootMs = millis();

  if (!LittleFS.begin()) {
    Serial0.println("LittleFS mount failed");
    return;
  }

  // ── API routes ──────────────────────────────────────
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/scan", HTTP_GET, handleScan);

  server.on("/save-clock", HTTP_POST, handleSaveClock);
  server.on("/save-wifi", HTTP_POST, handleSaveWifi);
  server.on("/save-hostname", HTTP_POST, handleSaveHostname);
  server.on("/save-display", HTTP_POST, handleSaveDisplay);
  server.on("/save-device", HTTP_POST, handleSaveDevice);
  server.on("/action", HTTP_POST, handleAction);

  server.on("/ota-url", HTTP_POST, handleOtaUrl);
  server.on("/check-update", HTTP_GET, handleCheckUpdate);
  server.on("/save-alarms", HTTP_POST, handleSaveAlarms);
  server.on("/save-alarms", HTTP_OPTIONS, []() { addCORSHeaders(); server.send(204); });

  // OPTIONS pre-flight for browsers
  server.on("/save-clock", HTTP_OPTIONS, []() { addCORSHeaders(); server.send(204); });
  server.on("/save-wifi", HTTP_OPTIONS, []() { addCORSHeaders(); server.send(204); });
  server.on("/save-hostname", HTTP_OPTIONS, []() { addCORSHeaders(); server.send(204); });
  server.on("/action", HTTP_OPTIONS, []() { addCORSHeaders(); server.send(204); });

  // OTA via HTTPUpdateServer (handles /ota_upload multipart POST)
  httpUpdater.setup(&server, "/ota");

  // ── Static portal files from embedded gz_web resources ──────────
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial0.println("Web server ready on port 80");
}

// ── web_loop ───────────────────────────────────────────────
void web_loop() {
  server.handleClient();
  doVersionFetch();
}

// ── AP / captive portal (setup mode) ──────────────────────
void startWiFiPortal() {
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial0.println("AP IP: " + WiFi.softAPIP().toString());
}