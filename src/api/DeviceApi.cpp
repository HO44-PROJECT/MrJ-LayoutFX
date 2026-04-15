/**
 * @file DeviceApi.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/DeviceApi.h"

#ifdef MRJFX_API_SERVER_ENABLED

  #include "api/ApiServer.h"
  #include "config/ConfigManager.h"
  #include <ArduinoJson.h>
  #include <LittleFS.h>
  #include <WiFi.h>
  #include <esp_system.h> // temperatureRead
  #ifdef MRJFX_SPI_CARDS_ENABLED
    #include "spi/Spi595Bus.h"
  #endif
  #ifdef MRJFX_OLED_ENABLED
    #include "oled/OledDisplay.h"
  #endif
  #ifdef MRJFX_SERIAL_SERVO_ENABLED
    #include "servo/SerialServoMotorMode.h"
  #endif
  #ifdef MRJFX_I2C_SCAN_ENABLED
    #include <Wire.h>
  #endif

  #define FIRMWARE_VERSION "v1"

// ---------------------------------------------------------------------------
// Static member
// ---------------------------------------------------------------------------

const DeviceFactory *DeviceApi::_factory = nullptr;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void DeviceApi::init(const DeviceFactory &factory) {
  _factory = &factory;

  ApiServer::on("/api/devices", HTTP_GET, _onGetDevices);
  ApiServer::on("/api/device", HTTP_POST, _onPostDevice);
  ApiServer::on("/api/switch", HTTP_POST, _onSwitch);
  ApiServer::on("/api/all", HTTP_POST, _onAllDevices);
  ApiServer::on("/api/group", HTTP_POST, _onGroupDevices);
  ApiServer::on("/api/config", HTTP_GET, _onGetConfig);
  ApiServer::on("/api/config/file", HTTP_GET, _onGetNamedConfig);
  ApiServer::on("/api/config", HTTP_POST, _onPostConfig);
  ApiServer::on("/api/config", HTTP_DELETE, _onDeleteConfig);
  ApiServer::on("/api/configs", HTTP_GET, _onGetConfigs);
  ApiServer::on("/api/configs", HTTP_POST, _onPostNamedConfig);
  ApiServer::on("/api/configs", HTTP_DELETE, _onDeleteNamedConfig);
  ApiServer::on("/api/config/copy", HTTP_POST, _onCopyConfig);
  ApiServer::on("/api/config/rename", HTTP_POST, _onRenameConfig);
  ApiServer::on("/api/config/activate", HTTP_POST, _onActivateConfig);
  ApiServer::on("/api/status", HTTP_GET, _onGetStatus);
  ApiServer::on("/api/boards", HTTP_GET, _onGetBoards);
  ApiServer::on("/api/board-types", HTTP_GET, _onGetBoardTypes);
  ApiServer::on("/api/health", HTTP_GET, _onGetHealth);
  ApiServer::on("/api/test/gpio", HTTP_POST, _onTestGpio);
  ApiServer::on("/api/test/spi", HTTP_POST, _onTestSpi);
  ApiServer::on("/api/restart", HTTP_POST, _onRestart);
  ApiServer::on("/api/servo", HTTP_POST, _onServo);
#ifdef MRJFX_I2C_SCAN_ENABLED
  ApiServer::on("/api/scan/i2c", HTTP_GET, _onScanI2c);
#endif

  // Collect custom upload header
  static const char *hdrs[] = {"X-Config-Name"};
  ApiServer::server().collectHeaders(hdrs, 1);
}

// ---------------------------------------------------------------------------
// Devices
// ---------------------------------------------------------------------------

void DeviceApi::_onGetDevices() {
  LOG_PRINTLN(F("API: GET /api/devices"));
  String json = "[";
  for (size_t i = 0; i < _factory->count(); i++) {
    Device *d = _factory->device(i);
    uint8_t board = _factory->deviceBoard(i);
    size_t pc = d->getPinCount();

    if (i > 0)
      json += ",";
    json += F("{\"id\":\"");
    json += _factory->deviceId(i);
    json += F("\",\"type\":\"");
    json += d->getDeviceName();
    json += F("\",\"state\":");
    json += (int)d->getState();
    json += F(",\"desired\":");
    json += (int)d->getDesiredState();
    json += F(",\"addr\":");
    json += (int)d->getDccAddress();
    json += F(",\"board\":");
    json += (int)board;
    json += F(",\"stateCount\":");
    json += (int)d->getStateCount();
    json += F(",\"pinCount\":");
    json += (int)pc;
    json += F(",\"pins\":[");
    for (size_t j = 0; j < pc; j++) {
      if (j > 0)
        json += ",";
  #ifdef MRJFX_SPI_CARDS_ENABLED
      json += (int)d->getPin(j).pin;
  #else
      json += (int)d->getPin(j);
  #endif
    }
    json += F("]");
  #ifdef MRJFX_SERIAL_SERVO_ENABLED
    if (strcmp_P("SerialServo", (const char *)d->getDeviceName()) == 0) {
      json += F(",\"servoId\":");
      json += (int)static_cast<SerialServoMotor *>(d)->getServoId();
    }
  #endif
    json += F("}");
  }
  json += "]";
  ApiServer::sendJson(200, json);
}

void DeviceApi::_onPostDevice() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["state"].is<int>()) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc["id"] | "";
  int state = doc["state"].as<int>();

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      Device *d = _factory->device(i);
      d->newState((STATE_TYPE)state);
  #ifdef MRJFX_OLED_ENABLED
      OledDisplay::notify(String(d->getDeviceName()).c_str(), id, state);
  #endif
      ApiServer::sendJson(200, F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::sendJson(404, F("{\"error\":\"device not found\"}"));
}

void DeviceApi::_onSwitch() {
  LOG_PRINTLN(F("API: POST /api/switch"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["on"].is<bool>()) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc["id"] | "";
  bool on = doc["on"].as<bool>();
  LOG_PRINTF("[switch] id='%s' on=%d\n", id, (int)on);

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      Device *d = _factory->device(i);
      if (on)
        d->switchOn();
      else
        d->switchOff();
  #ifdef MRJFX_OLED_ENABLED
      OledDisplay::notify(String(d->getDeviceName()).c_str(), id, on ? 1 : 0);
  #endif
      ApiServer::sendJson(200, F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::sendJson(404, F("{\"error\":\"device not found\"}"));
}

void DeviceApi::_onAllDevices() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["state"].is<int>()) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int state = doc["state"].as<int>();
  int board = doc["board"] | 0;

  for (size_t i = 0; i < _factory->count(); i++) {
    if (board > 0 && _factory->deviceBoard(i) != (uint8_t)board)
      continue;
    Device *d = _factory->device(i);
    if (strcmp("StaticLow", (const char *)d->getDeviceName()) != 0)
      d->newState((STATE_TYPE)state);
  }
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onGroupDevices() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["state"].is<int>()) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int state = doc["state"].as<int>();
  const char *type = doc["type"] | "";

  for (size_t i = 0; i < _factory->count(); i++) {
    Device *d = _factory->device(i);
    if (strcmp(type, (const char *)d->getDeviceName()) == 0)
      d->newState((STATE_TYPE)state);
  }
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

// ---------------------------------------------------------------------------
// Config (delegates to ConfigManager)
// ---------------------------------------------------------------------------

void DeviceApi::_onGetConfig() {
  LOG_PRINTLN(F("API: GET /api/config"));
  if (!ConfigManager::configExists()) {
    ApiServer::sendJson(404, F("{\"error\":\"config not found\"}"));
    return;
  }
  ApiServer::server().sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  ApiServer::sendJson(200, ConfigManager::readConfig());
}

void DeviceApi::_onPostConfig() {
  LOG_PRINTLN(F("API: POST /api/config"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  if (!ConfigManager::writeConfig(ApiServer::server().arg("plain"))) {
    ApiServer::sendJson(500, F("{\"error\":\"write failed\"}"));
    return;
  }
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onGetConfigs() {
  LOG_PRINTLN(F("API: GET /api/configs"));
  // Read the source file used for the last activation (written by activateConfig)
  String activeName;
  File src = LittleFS.exists("/config_source.txt") ? LittleFS.open("/config_source.txt", "r") : File();
  if (src) {
    activeName = src.readString();
    src.close();
    if (activeName.startsWith("/"))
      activeName = activeName.substring(1);
  }
  // Fallback: if no source file recorded yet, show config.json as active
  if (activeName.isEmpty()) {
    activeName = String(ConfigManager::configPath());
    if (activeName.startsWith("/"))
      activeName = activeName.substring(1);
  }
  String json = F("{\"active\":\"");
  json += activeName;
  json += F("\",\"files\":");
  json += ConfigManager::listConfigs();
  json += "}";
  ApiServer::sendJson(200, json);
}

// ---------------------------------------------------------------------------
// Config file management helpers
// ---------------------------------------------------------------------------

static bool _sanitizeCfgName(String &name); // forward declaration

// Write buf[0..len) to path in 512-byte chunks. Returns true if all bytes written.
static bool _writeAllBytes(File &f, const uint8_t *buf, size_t len) {
  const size_t CHUNK = 512;
  size_t offset = 0;
  while (offset < len) {
    size_t toWrite = min(len - offset, CHUNK);
    size_t w = f.write(buf + offset, toWrite);
    if (w == 0) {
      LOG_PRINTF("[FS] write stalled at offset=%u\n", offset);
      return false;
    }
    offset += w;
  }
  return true;
}

// Streaming file copy: read src in 512-byte chunks, write to dst. No full-String allocation.
static bool _copyFile(const char *src, const char *dst) {
  File in = LittleFS.open(src, "r");
  if (!in) {
    LOG_PRINTF("[FS] open(%s,r) failed\n", src);
    return false;
  }
  if (LittleFS.exists(dst))
    LittleFS.remove(dst);
  File out = LittleFS.open(dst, "w");
  if (!out) {
    in.close();
    LOG_PRINTF("[FS] open(%s,w) failed errno=%d\n", dst, errno);
    return false;
  }
  uint8_t buf[512];
  size_t total = 0;
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0) break;
    size_t w = out.write(buf, n);
    total += w;
    if (w != n) {
      LOG_PRINTF("[FS] write short: %u/%u at offset=%u\n", w, n, total);
      in.close(); out.close();
      return false;
    }
  }
  in.close();
  out.close();
  LOG_PRINTF("[FS] copyFile %s -> %s (%u bytes)\n", src, dst, total);
  return true;
}

static bool _safeRename(const char *from, const char *to) {
  if (!_copyFile(from, to))
    return false;
  LittleFS.remove(from);
  return true;
}

void DeviceApi::_onGetNamedConfig() {
  LOG_PRINTLN(F("API: GET /api/config/file"));
  String name = ApiServer::server().arg("name");
  if (!_sanitizeCfgName(name)) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  String path = "/" + name;
  if (!LittleFS.exists(path.c_str())) {
    ApiServer::sendJson(404, F("{\"error\":\"file not found\"}"));
    return;
  }
  String cd = "attachment; filename=\"";
  cd += name;
  cd += "\"";
  ApiServer::server().sendHeader("Content-Disposition", cd);
  ApiServer::server().sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  File f = LittleFS.open(path.c_str(), "r");
  ApiServer::server().streamFile(f, "application/json");
  f.close();
}

static bool _sanitizeCfgName(String &name) {
  if (!name.endsWith(".json"))
    name += ".json";
  if (name.length() > 32) // LFS_NAME_MAX = 32
    return false;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (!isAlphaNumeric(c) && c != '_' && c != '-' && c != '.')
      return false;
  }
  return true;
}

// ---------------------------------------------------------------------------

void DeviceApi::_onPostNamedConfig() {
  LOG_PRINTLN(F("API: POST /api/configs"));
  // Filename: prefer X-Config-Name header, fallback to ?name= URL param
  String name = ApiServer::server().header("X-Config-Name");
  if (name.isEmpty())
    name = ApiServer::server().arg("name");
  bool hasBody = ApiServer::server().hasArg("plain") && ApiServer::server().arg("plain").length() > 0;
  LOG_PRINTF("[upload] name='%s' hasBody=%d argCount=%d\n",
    name.c_str(), (int)hasBody, ApiServer::server().args());
  if (name.isEmpty() || !hasBody) {
    ApiServer::sendJson(400, F("{\"error\":\"filename and body required\"}"));
    return;
  }
  if (!_sanitizeCfgName(name)) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  if (name == "config.json") {
    ApiServer::sendJson(400, F("{\"error\":\"use POST /api/config to overwrite active config\"}"));
    return;
  }
  const String &content = ApiServer::server().arg("plain");
  String path = "/" + name;
  LOG_PRINTF("[upload] name=%s content_len=%u\n", name.c_str(), content.length());
  if (LittleFS.exists(path.c_str()))
    LittleFS.remove(path.c_str());
  errno = 0;
  File f = LittleFS.open(path.c_str(), "w");
  LOG_PRINTF("[upload] open(%s,w) ok=%d errno=%d\n", path.c_str(), (int)(bool)f, errno);
  if (!f) {
    ApiServer::sendJson(500, F("{\"error\":\"write failed\"}"));
    return;
  }
  bool ok = _writeAllBytes(f, (const uint8_t *)content.c_str(), content.length());
  f.close();
  LOG_PRINTF("[FS] saved %s ok=%d\n", path.c_str(), (int)ok);
  if (!ok) {
    ApiServer::sendJson(500, F("{\"error\":\"write incomplete\"}"));
    return;
  }
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onDeleteNamedConfig() {
  LOG_PRINTLN(F("API: DELETE /api/configs"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["file"].is<const char *>()) {
    ApiServer::sendJson(400, F("{\"error\":\"expected {file}\"}"));
    return;
  }
  String name = doc["file"].as<const char *>();
  if (!_sanitizeCfgName(name) || name == "config.json") {
    ApiServer::sendJson(400, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  String path = "/" + name;
  if (!LittleFS.exists(path.c_str())) {
    ApiServer::sendJson(404, F("{\"error\":\"file not found\"}"));
    return;
  }
  LittleFS.remove(path.c_str());
  LOG_PRINTF("[FS] deleted %s\n", path.c_str());
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onCopyConfig() {
  LOG_PRINTLN(F("API: POST /api/config/copy"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["from"].is<const char *>() || !doc["to"].is<const char *>()) {
    ApiServer::sendJson(400, F("{\"error\":\"expected {from, to}\"}"));
    return;
  }
  String fromName = doc["from"].as<const char *>();
  String toName = doc["to"].as<const char *>();
  if (!_sanitizeCfgName(fromName) || !_sanitizeCfgName(toName)) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  if (toName == "config.json") {
    ApiServer::sendJson(400, F("{\"error\":\"use /api/config/activate instead\"}"));
    return;
  }
  String fromPath = "/" + fromName;
  String toPath = "/" + toName;
  if (!LittleFS.exists(fromPath.c_str())) {
    ApiServer::sendJson(404, F("{\"error\":\"source not found\"}"));
    return;
  }
  if (!_copyFile(fromPath.c_str(), toPath.c_str())) {
    ApiServer::sendJson(500, F("{\"error\":\"write failed\"}"));
    return;
  }
  LOG_PRINTF("[FS] copied %s -> %s\n", fromPath.c_str(), toPath.c_str());
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onRenameConfig() {
  LOG_PRINTLN(F("API: POST /api/config/rename"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["from"].is<const char *>() || !doc["to"].is<const char *>()) {
    ApiServer::sendJson(400, F("{\"error\":\"expected {from, to}\"}"));
    return;
  }
  String fromName = doc["from"].as<const char *>();
  String toName = doc["to"].as<const char *>();
  if (!_sanitizeCfgName(fromName) || !_sanitizeCfgName(toName)) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  if (fromName == "config.json" || toName == "config.json") {
    ApiServer::sendJson(400, F("{\"error\":\"cannot rename active config\"}"));
    return;
  }
  String fromPath = "/" + fromName;
  String toPath = "/" + toName;
  if (!LittleFS.exists(fromPath.c_str())) {
    ApiServer::sendJson(404, F("{\"error\":\"source not found\"}"));
    return;
  }
  LOG_PRINTF("[rename] %s -> %s\n", fromPath.c_str(), toPath.c_str());
  if (!_safeRename(fromPath.c_str(), toPath.c_str())) {
    ApiServer::sendJson(500, F("{\"error\":\"rename failed\"}"));
    return;
  }
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onActivateConfig() {
  LOG_PRINTLN(F("API: POST /api/config/activate"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["file"].is<const char *>()) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  String file = "/";
  file += doc["file"].as<const char *>();
  if (!LittleFS.exists(file.c_str())) {
    ApiServer::sendJson(404, F("{\"error\":\"file not found\"}"));
    return;
  }
  if (!ConfigManager::activateConfig(file.c_str())) {
    ApiServer::sendJson(500, F("{\"error\":\"copy failed\"}"));
    return;
  }
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onDeleteConfig() {
  LOG_PRINTLN(F("API: DELETE /api/config"));
  ConfigManager::deleteConfig();
  ApiServer::sendJson(200, F("{\"ok\":true}"));
  delay(200);
  ESP.restart();
}

void DeviceApi::_onServo() {
  LOG_PRINTLN(F("API: POST /api/servo"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain"))) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc["id"] | "";
  const char *action = doc["action"] | "";
  bool isReverse = strcmp(action, "reverse") == 0;
  if (!isReverse && !doc["speed"].is<int>()) {
    ApiServer::sendJson(400, F("{\"error\":\"speed or action required\"}"));
    return;
  }

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      if (isReverse)
        _factory->device(i)->reverseMotor();
      else
        _factory->device(i)->setMotorSpeed((int16_t)doc["speed"].as<int>());
      ApiServer::sendJson(200, F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::sendJson(404, F("{\"error\":\"device not found\"}"));
}

void DeviceApi::_onRestart() {
  LOG_PRINTLN(F("API: POST /api/restart"));
  ApiServer::sendJson(200, F("{\"ok\":true}"));
#ifdef MRJFX_OLED_ENABLED
  OledDisplay::showMessage("MrJ RailwayFX", "Redemarrage...");
#endif
  delay(400);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

void DeviceApi::_onGetStatus() {
  LOG_PRINTLN(F("API: GET /api/status"));
  JsonDocument doc;

  doc["version"] = FIRMWARE_VERSION;
  doc["build_date"] = __DATE__ " " __TIME__;
#ifdef PIOENV_NAME
  doc["env"] = F(PIOENV_NAME);
#endif
  doc["uptime_s"] = millis() / 1000UL;
  doc["ip"] = WiFi.localIP().toString();
  doc["config"] = ConfigManager::configExists();
  doc["devices"]     = (int)_factory->count();
  doc["devices_max"] = (int)FACTORY_MAX_DEVICES;
  doc["cpu_mhz"] = ESP.getCpuFreqMHz();
  doc["chip"] = ESP.getChipModel();
  doc["chip_rev"] = ESP.getChipRevision();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_total"] = ESP.getHeapSize();
  doc["heap_min"] = ESP.getMinFreeHeap();
  doc["temp_c"] = temperatureRead();
  doc["sketch_size"] = ESP.getSketchSize();
  doc["sketch_free"] = ESP.getFreeSketchSpace();
  doc["fs_total"] = LittleFS.totalBytes();
  doc["fs_used"] = LittleFS.usedBytes();

  JsonObject feat = doc["features"].to<JsonObject>();
  feat["wifi"]        = true;  // always true (API requires WiFi)
  feat["api"]         = true;  // always true (this endpoint exists)
  feat["webui"]       = true;  // always true (this page is served)
  feat["config"]      = true;  // always true (API requires config)
#ifdef MRJFX_OLED_ENABLED
  feat["oled"]        = true;
#else
  feat["oled"]        = false;
#endif
#ifdef MRJFX_SPI_CARDS_ENABLED
  feat["spi"]         = true;
#else
  feat["spi"]         = false;
#endif
#ifdef MRJFX_LOBOT_SERVO_ENABLED
  feat["lobot_servo"] = true;
#else
  feat["lobot_servo"] = false;
#endif
#ifdef MRJFX_LX16A_SERVO_ENABLED
  feat["lx16a_servo"] = true;
#else
  feat["lx16a_servo"] = false;
#endif
#ifdef MRJFX_DCC_ENABLED
  feat["dcc"]         = true;
#else
  feat["dcc"]         = false;
#endif
#ifdef MRJFX_AUDIO_ENABLED
  feat["audio"]       = true;
#else
  feat["audio"]       = false;
#endif

  // Pins reserved by compile-time features (not declared in config.json)
#if defined(LOG_SERIAL) || defined(DEBUG_SERIAL) || defined(MRJFX_OLED_ENABLED)
  {
    JsonObject sp = doc["sys_pins"].to<JsonObject>();
#if defined(LOG_SERIAL) || defined(DEBUG_SERIAL)
    sp[String(1)] = F("TX0");
    sp[String(3)] = F("RX0");
#endif
#ifdef MRJFX_OLED_ENABLED
    sp[String(OLED_SDA)] = F("SDA");
    sp[String(OLED_SCL)] = F("SCL");
#endif
  }
#endif

  String json;
  serializeJson(doc, json);
  ApiServer::sendJson(200, json);
}

// ---------------------------------------------------------------------------
// Boards
// ---------------------------------------------------------------------------

void DeviceApi::_onGetBoards() {
  LOG_PRINTLN(F("API: GET /api/boards"));
  String json = "[";
  for (uint8_t i = 1; i <= _factory->boardCount(); i++) {
    const DeviceFactory::BoardCfg &b = _factory->board(i);
    if (i > 1)
      json += ",";
    json += F("{\"id\":\"");
    json += b.id;
    json += F("\",\"type\":\"");
    json += b.typeStr;
    json += F("\",\"bus\":\"");
    json += b.busKey;
    json += F("\",\"pinCount\":");
    json += (int)b.pinCount;
    json += F(",\"spiRank\":");
    json += (int)b.spiRank;
    json += F("}");
  }
  json += "]";
  ApiServer::sendJson(200, json);
}

void DeviceApi::_onGetBoardTypes() {
  LOG_PRINTLN(F("API: GET /api/board-types"));
  if (!LittleFS.exists("/board_types.json")) {
    ApiServer::sendJson(404, F("{\"error\":\"board_types.json not found\"}"));
    return;
  }
  File f = LittleFS.open("/board_types.json", "r");
  ApiServer::server().streamFile(f, "application/json");
  f.close();
}

// ---------------------------------------------------------------------------
// Raw hardware tests
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Health check
// ---------------------------------------------------------------------------

void DeviceApi::_onGetHealth() {
  String json = "[";
  for (size_t i = 0; i < _factory->count(); i++) {
    Device *d = _factory->device(i);
    if (i > 0)
      json += ",";
    json += F("{\"id\":\"");
    json += _factory->deviceId(i);
    json += F("\",\"desired\":");
    json += (int)d->getDesiredState();
    json += F(",\"state\":");
    json += (int)d->getState();
    int result = d->healthCheck();
    if (result != -1) {
      json += F(",\"ok\":");
      json += (result == 0) ? F("true") : F("false");
      if (result > 0) {
        json += F(",\"error\":");
        json += result;
      } else {
        d->appendHealthJson(json);
      }
    }
    json += "}";
  }
  json += "]";
  ApiServer::sendJson(200, json);
}

// ---------------------------------------------------------------------------
// Raw hardware tests
// ---------------------------------------------------------------------------

void DeviceApi::_onTestGpio() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["pin"].is<int>()) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int pin = doc["pin"].as<int>();
  int state = doc["state"] | 0;
  if (pin < 0 || pin > 39) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid pin\"}"));
    return;
  }
  if (pin == 1 || pin == 3) {
    ApiServer::sendJson(403, F("{\"error\":\"reserved UART pin\"}"));
    return;
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, state ? HIGH : LOW);
  ApiServer::sendJson(200, F("{\"ok\":true}"));
}

void DeviceApi::_onTestSpi() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::sendJson(400, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["card"].is<int>() || !doc["channel"].is<int>()) {
    ApiServer::sendJson(400, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int card = doc["card"].as<int>();
  int channel = doc["channel"].as<int>();
  int state = doc["state"] | 0;
  #ifdef MRJFX_SPI_CARDS_ENABLED
  if (!Spi595Bus::ready()) {
    ApiServer::sendJson(503, F("{\"error\":\"SPI not ready\"}"));
    return;
  }
  Spi595Bus::setPin((uint8_t)card, (uint8_t)channel, (uint8_t)(state ? 1 : 0));
  ApiServer::sendJson(200, F("{\"ok\":true}"));
  #else
  (void)card;
  (void)channel;
  (void)state;
  ApiServer::sendJson(501, F("{\"error\":\"SPI not enabled\"}"));
  #endif
}

// ---------------------------------------------------------------------------
// I2C scanner
// ---------------------------------------------------------------------------

#ifdef MRJFX_I2C_SCAN_ENABLED
void DeviceApi::_onScanI2c() {
  LOG_PRINTLN(F("API: GET /api/scan/i2c"));
#ifdef MRJFX_OLED_ENABLED
  const int sda = OLED_SDA, scl = OLED_SCL;
#else
  const int sda = 21, scl = 22;
#endif
  Wire.begin(sda, scl);

  JsonDocument doc;
  doc["sda"] = sda;
  doc["scl"] = scl;
  JsonArray found = doc["found"].to<JsonArray>();
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) found.add(addr);
  }
  doc["count"] = found.size();

  String json;
  serializeJson(doc, json);
  ApiServer::sendJson(200, json);
}
#endif

#endif // MRJFX_API_SERVER_ENABLED
