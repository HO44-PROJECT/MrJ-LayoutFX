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

  ApiServer::on("/api/devices",    HTTP_GET,    _onGetDevices);
  ApiServer::on("/api/device",     HTTP_POST,   _onPostDevice);
  ApiServer::on("/api/switch",     HTTP_POST,   _onSwitch);
  ApiServer::on("/api/all",        HTTP_POST,   _onAllDevices);
  ApiServer::on("/api/group",      HTTP_POST,   _onGroupDevices);
  ApiServer::on("/api/config",     HTTP_GET,    _onGetConfig);
  ApiServer::on("/api/config",     HTTP_POST,   _onPostConfig);
  ApiServer::on("/api/config",     HTTP_DELETE, _onDeleteConfig);
  ApiServer::on("/api/status",     HTTP_GET,    _onGetStatus);
  ApiServer::on("/api/boards",     HTTP_GET,    _onGetBoards);
  ApiServer::on("/api/board-types",HTTP_GET,    _onGetBoardTypes);
  ApiServer::on("/api/test/gpio",  HTTP_POST,   _onTestGpio);
  ApiServer::on("/api/test/spi",   HTTP_POST,   _onTestSpi);
}

// ---------------------------------------------------------------------------
// Devices
// ---------------------------------------------------------------------------

void DeviceApi::_onGetDevices() {
  Serial.println(F("API: GET /api/devices"));
  String json = "[";
  for (size_t i = 0; i < _factory->count(); i++) {
    Device *d = _factory->device(i);
    uint8_t board = _factory->deviceBoard(i);
    size_t pc = d->getPinCount();

    if (i > 0) json += ",";
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
      if (j > 0) json += ",";
    #ifdef MRJFX_SPI_CARDS_ENABLED
      json += (int)d->getPin(j).pin;
    #else
      json += (int)d->getPin(j);
    #endif
    }
    json += F("]}");
  }
  json += "]";
  ApiServer::server().send(200, "application/json", json);
}

void DeviceApi::_onPostDevice() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["state"].is<int>()) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc["id"] | "";
  int state = doc["state"].as<int>();

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      Device* d = _factory->device(i);
      d->newState((STATE_TYPE)state);
#ifdef MRJFX_OLED_ENABLED
      OledDisplay::notify(String(d->getDeviceName()).c_str(), id, state);
#endif
      ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::server().send(404, "application/json", F("{\"error\":\"device not found\"}"));
}

void DeviceApi::_onSwitch() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["on"].is<bool>()) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc["id"] | "";
  bool on = doc["on"].as<bool>();

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      Device* d = _factory->device(i);
      if (on) d->switchOn();
      else    d->switchOff();
#ifdef MRJFX_OLED_ENABLED
      OledDisplay::notify(String(d->getDeviceName()).c_str(), id, on ? 1 : 0);
#endif
      ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::server().send(404, "application/json", F("{\"error\":\"device not found\"}"));
}

void DeviceApi::_onAllDevices() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["state"].is<int>()) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
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
  ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

void DeviceApi::_onGroupDevices() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["state"].is<int>()) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int state = doc["state"].as<int>();
  const char *type = doc["type"] | "";

  for (size_t i = 0; i < _factory->count(); i++) {
    Device *d = _factory->device(i);
    if (strcmp(type, (const char *)d->getDeviceName()) == 0)
      d->newState((STATE_TYPE)state);
  }
  ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

// ---------------------------------------------------------------------------
// Config (delegates to ConfigManager)
// ---------------------------------------------------------------------------

void DeviceApi::_onGetConfig() {
  Serial.println(F("API: GET /api/config"));
  if (!ConfigManager::configExists()) {
    ApiServer::server().send(404, "application/json", F("{\"error\":\"config not found\"}"));
    return;
  }
  ApiServer::server().sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  ApiServer::server().send(200, "application/json", ConfigManager::readConfig());
}

void DeviceApi::_onPostConfig() {
  Serial.println(F("API: POST /api/config"));
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
    return;
  }
  if (!ConfigManager::writeConfig(ApiServer::server().arg("plain"))) {
    ApiServer::server().send(500, "application/json", F("{\"error\":\"write failed\"}"));
    return;
  }
  ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
  delay(300);
  ESP.restart();
}

void DeviceApi::_onDeleteConfig() {
  Serial.println(F("API: DELETE /api/config"));
  ConfigManager::deleteConfig();
  ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
  delay(200);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

void DeviceApi::_onGetStatus() {
  Serial.println(F("API: GET /api/status"));
  JsonDocument doc;

  doc["version"]    = FIRMWARE_VERSION;
  doc["build_date"] = __DATE__ " " __TIME__;
  doc["uptime_s"]   = millis() / 1000UL;
  doc["ip"]         = WiFi.localIP().toString();
  doc["config"]     = ConfigManager::configExists();
  doc["devices"]    = (int)_factory->count();
  doc["cpu_mhz"]    = ESP.getCpuFreqMHz();
  doc["chip"]       = ESP.getChipModel();
  doc["chip_rev"]   = ESP.getChipRevision();
  doc["heap_free"]  = ESP.getFreeHeap();
  doc["heap_total"] = ESP.getHeapSize();
  doc["heap_min"]   = ESP.getMinFreeHeap();
  doc["temp_c"]     = temperatureRead();
  doc["sketch_size"]= ESP.getSketchSize();
  doc["sketch_free"]= ESP.getFreeSketchSpace();
  doc["fs_total"]   = LittleFS.totalBytes();
  doc["fs_used"]    = LittleFS.usedBytes();

  String json;
  serializeJson(doc, json);
  ApiServer::server().send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Boards
// ---------------------------------------------------------------------------

void DeviceApi::_onGetBoards() {
  Serial.println(F("API: GET /api/boards"));
  String json = "[";
  for (uint8_t i = 1; i <= _factory->boardCount(); i++) {
    const DeviceFactory::BoardCfg &b = _factory->board(i);
    if (i > 1) json += ",";
    json += F("{\"id\":\"");   json += b.id;
    json += F("\",\"type\":\""); json += b.typeStr;
    json += F("\",\"bus\":\"");  json += b.busKey;
    json += F("\",\"pinCount\":"); json += (int)b.pinCount;
    json += F(",\"spiRank\":"); json += (int)b.spiRank;
    json += F("}");
  }
  json += "]";
  ApiServer::server().send(200, "application/json", json);
}

void DeviceApi::_onGetBoardTypes() {
  Serial.println(F("API: GET /api/board-types"));
  if (!LittleFS.exists("/board_types.json")) {
    ApiServer::server().send(404, "application/json", F("{\"error\":\"board_types.json not found\"}"));
    return;
  }
  File f = LittleFS.open("/board_types.json", "r");
  ApiServer::server().streamFile(f, "application/json");
  f.close();
}

// ---------------------------------------------------------------------------
// Raw hardware tests
// ---------------------------------------------------------------------------

void DeviceApi::_onTestGpio() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain")) || !doc["pin"].is<int>()) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int pin   = doc["pin"].as<int>();
  int state = doc["state"] | 0;
  if (pin < 0 || pin > 39) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid pin\"}"));
    return;
  }
  if (pin == 1 || pin == 3) {
    ApiServer::server().send(403, "application/json", F("{\"error\":\"reserved UART pin\"}"));
    return;
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, state ? HIGH : LOW);
  ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

void DeviceApi::_onTestSpi() {
  if (!ApiServer::server().hasArg("plain")) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg("plain"))
      || !doc["card"].is<int>() || !doc["channel"].is<int>()) {
    ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int card    = doc["card"].as<int>();
  int channel = doc["channel"].as<int>();
  int state   = doc["state"] | 0;
  #ifdef MRJFX_SPI_CARDS_ENABLED
  if (!Spi595Bus::ready()) {
    ApiServer::server().send(503, "application/json", F("{\"error\":\"SPI not ready\"}"));
    return;
  }
  Spi595Bus::setPin((uint8_t)card, (uint8_t)channel, (uint8_t)(state ? 1 : 0));
  ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
  #else
  (void)card; (void)channel;
  ApiServer::server().send(501, "application/json", F("{\"error\":\"SPI not enabled\"}"));
  #endif
}

#endif  // MRJFX_API_SERVER_ENABLED
