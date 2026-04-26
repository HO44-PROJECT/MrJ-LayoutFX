/**
 * @file DeviceStatusApi.cpp
 * @brief DeviceApi — system introspection endpoints (/api/status, /api/boards,
 *        /api/board-types, /api/health, /api/restart).
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/DeviceApi.h"
#include "api/build_info.h"

#ifdef MRJFX_API_SERVER_ENABLED

using namespace api_keys;
using namespace http_status;

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

/**
 * @brief Return a JSON object with full system metrics: firmware version, IP,
 *        heap, LittleFS usage, CPU, chip info, feature flags, lib versions,
 *        WiFi details, and reserved system pins.
 */
void DeviceApi::_onGetStatus() {
  LOG_PRINTLN(F("API: GET /api/status"));
  JsonDocument doc;

  doc[kVersion] = MRJFX_FIRMWARE_VERSION;
  doc[kBuildDate] = __DATE__ " " __TIME__;
  #ifdef PIOENV_NAME
  doc[kEnv] = F(PIOENV_NAME);
  #endif
  doc[kUptimeS] = millis() / 1000UL;
  doc[kIp] = WiFi.localIP().toString();
  doc[kConfig] = ConfigManager::configExists();
  doc[kDevices] = (int)_factory->count();
  doc[kDevicesMax] = (int)MRJFX_FACTORY_MAX_DEVICES;
  doc[kCpuMhz] = ESP.getCpuFreqMHz();
  doc[kChip] = ESP.getChipModel();
  doc[kChipRev] = ESP.getChipRevision();
  doc[kHeapFree] = ESP.getFreeHeap();
  doc[kHeapTotal] = ESP.getHeapSize();
  doc[kHeapMin] = ESP.getMinFreeHeap();
  doc[kTempC] = temperatureRead();
  doc[kSketchSize] = ESP.getSketchSize();
  doc[kSketchFree] = ESP.getFreeSketchSpace();
  doc[kFsTotal] = LittleFS.totalBytes();
  doc[kFsUsed] = LittleFS.usedBytes();

  JsonObject feat = doc[kFeatures].to<JsonObject>();
  feat[kFeatWifi] = true;
  feat[kFeatApi] = true;
  feat[kFeatWebui] = true;
  feat[kFeatConfig] = true;
  #ifdef MRJFX_OLED_ENABLED
  feat[kFeatOled] = true;
  #else
  feat[kFeatOled] = false;
  #endif
  #ifdef MRJFX_SPI_CARDS_ENABLED
  feat[kFeatSpi] = true;
  #else
  feat[kFeatSpi] = false;
  #endif
  #ifdef MRJFX_LOBOT_SERVO_ENABLED
  feat[kFeatLobotServo] = true;
  #else
  feat[kFeatLobotServo] = false;
  #endif
  #ifdef MRJFX_LX16A_SERVO_ENABLED
  feat[kFeatLx16aServo] = true;
  #else
  feat[kFeatLx16aServo] = false;
  #endif
  #ifdef MRJFX_DCC_ENABLED
  feat[kFeatDcc] = true;
  #else
  feat[kFeatDcc] = false;
  #endif
  #ifdef MRJFX_AUDIO_ENABLED
  feat[kFeatAudio] = true;
  #else
  feat[kFeatAudio] = false;
  #endif

  #if defined(LOG_SERIAL) || defined(DEBUG_SERIAL) || defined(MRJFX_DCC_ENABLED)
  {
    JsonObject sp = doc[kSysPins].to<JsonObject>();
    #if defined(LOG_SERIAL) || defined(DEBUG_SERIAL)
    sp[String(1)] = kPinTx0;
    sp[String(3)] = kPinRx0;
    #endif
    #ifdef MRJFX_DCC_ENABLED
    sp[String(DCC_PIN)] = kPinDcc;
    #endif
  }
  #endif

  doc[kWifiSsid] = WiFi.SSID();
  doc[kWifiRssi] = WiFi.RSSI();
  doc[kWifiMac] = WiFi.macAddress();

  #define _MRJFX_STR_(x) #x
  #define _MRJFX_STR(x) _MRJFX_STR_(x)
  static const struct {
    const char *name;
    const char *runtime;
  } kRuntimeVers[] = {
  #ifdef ACE_ROUTINE_VERSION_STR
      {"AceRoutine", ACE_ROUTINE_VERSION_STR},
  #endif
      {"ArduinoJson", ARDUINOJSON_VERSION},
  #ifdef NMRADCC_VERSION
      {"NmraDcc", _MRJFX_STR(NMRADCC_VERSION)},
  #endif
  #ifdef U8G2_VERSION
      {"U8g2", U8G2_VERSION},
  #endif
      {nullptr, nullptr}};
  #undef _MRJFX_STR_
  #undef _MRJFX_STR

  JsonObject libs = doc[kLibs].to<JsonObject>();
  libs[kLibEspIdf] = esp_get_idf_version();
  #ifdef ESP_ARDUINO_VERSION_STR
  libs[kLibArduinoEsp32] = ESP_ARDUINO_VERSION_STR;
  #endif
  for (int i = 0; kLibDeps[i].name; i++) {
    String val = kLibDeps[i].ver;
    for (int j = 0; kRuntimeVers[j].name; j++) {
      if (strcmp(kLibDeps[i].name, kRuntimeVers[j].name) == 0 && kRuntimeVers[j].runtime) {
        val += " (";
        val += kRuntimeVers[j].runtime;
        val += ")";
        break;
      }
    }
    libs[kLibDeps[i].name] = val;
  }

  String json;
  serializeJson(doc, json);
  ApiServer::sendJson(kOk, json);
}

// ---------------------------------------------------------------------------
// Boards
// ---------------------------------------------------------------------------

/** @brief Return a JSON array of configured boards with id, type, bus, pinCount, spiRank. */
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
  ApiServer::sendJson(kOk, json);
}

/** @brief Stream a JSON file from LittleFS. Sends 404 if the file is absent. */
static void _streamJsonFile(const char *path, const char *filename) {
  if (!LittleFS.exists(path)) {
    String err = F("{\"error\":\"");
    err += filename;
    err += F(" not found\"}");
    ApiServer::sendJson(kNotFound, err);
    return;
  }
  File f = LittleFS.open(path, "r");
  ApiServer::server().streamFile(f, "application/json");
  f.close();
}

void DeviceApi::_onGetBoardTypes() {
  LOG_PRINTLN(F("API: GET /api/board-types"));
  _streamJsonFile(kPathBoardTypes, kFileBoardTypes);
}
void DeviceApi::_onGetDeviceTypes() {
  LOG_PRINTLN(F("API: GET /api/device-types"));
  _streamJsonFile(kPathDeviceTypes, kFileDeviceTypes);
}
void DeviceApi::_onGetBusTypes() {
  LOG_PRINTLN(F("API: GET /api/bus-types"));
  _streamJsonFile(kPathBusTypes, kFileBusTypes);
}
void DeviceApi::_onGetI2cKnown() {
  LOG_PRINTLN(F("API: GET /api/i2c-known"));
  _streamJsonFile(kPathI2cKnown, kFileI2cKnown);
}

// ---------------------------------------------------------------------------
// Health
// ---------------------------------------------------------------------------

/**
 * @brief Return a JSON array of per-device health check results.
 *        Each entry includes id, state, desired, and (if supported) ok flag
 *        plus any device-specific JSON fields from appendHealthJson().
 */
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
  ApiServer::sendJson(kOk, json);
}

// ---------------------------------------------------------------------------
// Restart
// ---------------------------------------------------------------------------

/** @brief Send a 200 OK response then trigger an immediate ESP32 restart. */
void DeviceApi::_onRestart() {
  LOG_PRINTLN(F("API: POST /api/restart"));
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
  #ifdef MRJFX_OLED_ENABLED
  OledDisplay::showMessage("MrJ RailwayFX", "Redemarrage...");
  #endif
  delay(400);
  ESP.restart();
}

#endif // MRJFX_API_SERVER_ENABLED
