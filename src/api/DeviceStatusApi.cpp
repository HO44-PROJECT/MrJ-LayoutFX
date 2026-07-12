/**
 * @file DeviceStatusApi.cpp
 * @brief DeviceApi — system introspection endpoints (/api/status, /api/boards,
 *        /api/board-types, /api/health, /api/restart).
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/DeviceApi.h"
#include "generated/build_info.h"

#ifdef LFX_API_SERVER_ENABLED

// Embedded JSON catalogs (PROGMEM, gzipped)
#include "generated/embedded_board_types.h"
#include "generated/embedded_device_types.h"
#include "generated/embedded_bus_types.h"
#include "generated/embedded_i2c_known.h"
#include "utils/utils.h" // g_lfxLogActive (runtime UART0 log-bus state)

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

  doc[kVersion] = LFX_FIRMWARE_VERSION;
  doc[kBuildDate] = kFirmwareBuildDate;
  #ifdef PIOENV_NAME
  doc[kEnv] = F(PIOENV_NAME);
  #endif
  doc[kUptimeS] = millis() / 1000UL;
  bool ap = ApiServer::isAP();
  doc[kIp]       = ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc[F("wifiMode")] = ap ? F("ap") : F("sta");
  doc[kConfig] = ConfigManager::configExists();
  doc[kDevices] = (int)_factory->count();
  doc[kDevicesMax] = (int)LFX_FACTORY_MAX_DEVICES;
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
  #ifdef LFX_OLED_ENABLED
  feat[kFeatOled] = true;
  #else
  feat[kFeatOled] = false;
  #endif
  #ifdef LFX_I2C_DEVICES_ENABLED
  feat[kFeatI2c] = true;
  #else
  feat[kFeatI2c] = false;
  #endif
  #ifdef LFX_SPI_CARDS_ENABLED
  feat[kFeatSpi] = true;
  #else
  feat[kFeatSpi] = false;
  #endif
  #ifdef LFX_LOBOT_SERVO_ENABLED
  feat[kFeatLobotServo] = true;
  #else
  feat[kFeatLobotServo] = false;
  #endif
  #ifdef LFX_LX16A_SERVO_ENABLED
  feat[kFeatLx16aServo] = true;
  #else
  feat[kFeatLx16aServo] = false;
  #endif
  #ifdef LFX_DCC_ENABLED
  feat[kFeatDcc] = true;
  #else
  feat[kFeatDcc] = false;
  #endif
  #ifdef LFX_AUDIO_ENABLED
  feat[kFeatAudio] = true;
  #else
  feat[kFeatAudio] = false;
  #endif
  #ifdef LFX_OTA_ENABLED
  feat[kFeatOta] = true;
  #else
  feat[kFeatOta] = false;
  #endif
  // Each badge below mirrors a compile flag (see LayoutFX_define.h) — "built",
  // not "active". The uart0 bus card shows whether serial logging is live.
  // ── Logging sinks ──
  #ifdef LOG_SERIAL
  feat[kFeatLogSerial] = true;
  #else
  feat[kFeatLogSerial] = false;
  #endif
  #ifdef DEBUG_SERIAL
  feat[kFeatDebugSerial] = true;
  #else
  feat[kFeatDebugSerial] = false;
  #endif
  #ifdef LOG_OLED
  feat[kFeatLogOled] = true;
  #else
  feat[kFeatLogOled] = false;
  #endif
  #ifdef DEBUG_OLED
  feat[kFeatDebugOled] = true;
  #else
  feat[kFeatDebugOled] = false;
  #endif
  // ── OLED options ──
  #ifdef OLED_STATUS
  feat[kFeatOledStatus] = true;
  #else
  feat[kFeatOledStatus] = false;
  #endif
  #ifdef LFX_OLED_SPLASH_ENABLED
  feat[kFeatOledSplash] = true;
  #else
  feat[kFeatOledSplash] = false;
  #endif
  #ifdef OLED_DEBUG_METRICS
  feat[kFeatOledMetrics] = true;
  #else
  feat[kFeatOledMetrics] = false;
  #endif
  #ifdef OLED_DEBUG_EVENTS
  feat[kFeatOledEvents] = true;
  #else
  feat[kFeatOledEvents] = false;
  #endif
  // ── Network / bus / behaviour options ──
  #ifdef LFX_WIFI_FORCE_AP
  feat[kFeatWifiForceAp] = true;
  #else
  feat[kFeatWifiForceAp] = false;
  #endif
  #ifdef LFX_DCC_AUDIT_ENABLED
  feat[kFeatDccAudit] = true;
  #else
  feat[kFeatDccAudit] = false;
  #endif
  #ifdef SERVO_PRESERVE_DIRECTION
  feat[kFeatServoDir] = true;
  #else
  feat[kFeatServoDir] = false;
  #endif
  #ifdef LFX_I2C_SCAN_ENABLED
  feat[kFeatI2cScan] = true;
  #else
  feat[kFeatI2cScan] = false;
  #endif
  #ifdef USE_JTAG
  feat[kFeatJtag] = true;
  #else
  feat[kFeatJtag] = false;
  #endif
  #ifdef DEMO
  feat[kFeatDemo] = true;
  #else
  feat[kFeatDemo] = false;
  #endif

  #if defined(LOG_SERIAL) || defined(DEBUG_SERIAL) || defined(LFX_DCC_ENABLED)
  {
    JsonObject sp = doc[kSysPins].to<JsonObject>();
    #if defined(LOG_SERIAL)
    if (g_lfxLogActive) { // runtime: 1/3 reserved only while the uart0 log bus is active
      sp[String(1)] = kPinTx0;
      sp[String(3)] = kPinRx0;
    }
    #elif defined(DEBUG_SERIAL)
    sp[String(1)] = kPinTx0;
    sp[String(3)] = kPinRx0;
    #endif
    #ifdef LFX_DCC_ENABLED
    sp[String(DCC_PIN)] = kPinDcc;
    #endif
  }
  #endif

  doc[kWifiSsid] = ap ? WiFi.softAPSSID() : WiFi.SSID();
  doc[kWifiRssi] = ap ? 0                 : WiFi.RSSI();
  doc[kWifiMac]  = WiFi.macAddress();

  #define _LFX_STR_(x) #x
  #define _LFX_STR(x) _LFX_STR_(x)
  static const struct {
    const char *name;
    const char *runtime;
  } kRuntimeVers[] = {
  #ifdef ACE_ROUTINE_VERSION_STR
      {"AceRoutine", ACE_ROUTINE_VERSION_STR},
  #endif
      {"ArduinoJson", ARDUINOJSON_VERSION},
  #ifdef NMRADCC_VERSION
      {"NmraDcc", _LFX_STR(NMRADCC_VERSION)},
  #endif
  #ifdef U8G2_VERSION
      {"U8g2", U8G2_VERSION},
  #endif
      {nullptr, nullptr}};
  #undef _LFX_STR_
  #undef _LFX_STR

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

/**
 * @brief Serve embedded gzipped JSON from PROGMEM.
 * @param data Pointer to gzipped JSON data in PROGMEM
 * @param len  Length of gzipped data
 */
static void _serveEmbeddedJson(const uint8_t *data, size_t len) {
  ApiServer::server().sendHeader("Cache-Control", "max-age=86400"); // 24h cache
  ApiServer::server().sendHeader("Content-Encoding", "gzip");
  ApiServer::server().send_P(200, "application/json", (const char *)data, len);
}

void DeviceApi::_onGetBoardTypes() {
  LOG_PRINTLN(F("API: GET /api/board-types (PROGMEM)"));
  _serveEmbeddedJson(BOARD_TYPES_GZ, BOARD_TYPES_GZ_LEN);
}
void DeviceApi::_onGetDeviceTypes() {
  LOG_PRINTLN(F("API: GET /api/device-types (PROGMEM)"));
  _serveEmbeddedJson(DEVICE_TYPES_GZ, DEVICE_TYPES_GZ_LEN);
}
void DeviceApi::_onGetBusTypes() {
  LOG_PRINTLN(F("API: GET /api/bus-types (PROGMEM)"));
  _serveEmbeddedJson(BUS_TYPES_GZ, BUS_TYPES_GZ_LEN);
}
void DeviceApi::_onGetI2cKnown() {
  LOG_PRINTLN(F("API: GET /api/i2c-known (PROGMEM)"));
  _serveEmbeddedJson(I2C_KNOWN_GZ, I2C_KNOWN_GZ_LEN);
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
// DCC status
// ---------------------------------------------------------------------------

/**
 * @brief Return per-category DCC packet counters and last-seen timestamps.
 *        Powers the WebUI Diagnostics tab's live DCC activity indicators —
 *        lets the user tell "bus dead" (nothing, not even `raw`, ever increments)
 *        from "bus alive but wrong address/CV" (raw increments, others don't).
 */
void DeviceApi::_onGetDccStatus() {
  LOG_PRINTLN(F("API: GET /api/dcc-status"));
  JsonDocument doc;

  #ifdef LFX_DCC_ENABLED
  doc[kDccEnabled] = true;
  doc[kDccUptimeMs] = millis();
  JsonObject msgs = doc[kDccMessages].to<JsonObject>();
  static const struct { const char *key; DccDrivable::DccMsgKind kind; } kKinds[] = {
    {kDccKindRaw, DccDrivable::DCC_MSG_RAW},
    {kDccKindSpeed, DccDrivable::DCC_MSG_SPEED},
    {kDccKindFunc, DccDrivable::DCC_MSG_FUNC},
    {kDccKindAccessory, DccDrivable::DCC_MSG_ACCESSORY},
    {kDccKindSignal, DccDrivable::DCC_MSG_SIGNAL},
  };
  for (const auto &k : kKinds) {
    JsonObject o = msgs[k.key].to<JsonObject>();
    o[kDccCount] = DccDrivable::dccMsgCountOf(k.kind);
    o[kDccLastMs] = DccDrivable::dccMsgLastMsOf(k.kind);
  }

  // Each category (Speed/Func/Accessory/Signal) keeps its own ring buffer (see logDccEvent's
  // doc comment for why), so merge them here by timestamp for the "all" view — at most
  // DCC_MSG_KIND_COUNT x DCC_LOG_CAPACITY entries, cheap to merge with a simple repeated
  // linear scan even on a Nano.
  const DccDrivable::DccLogEntry *merged[(DccDrivable::DCC_MSG_KIND_COUNT - 1) * DccDrivable::DCC_LOG_CAPACITY];
  uint8_t mergedCount = 0;
  uint8_t next[DccDrivable::DCC_MSG_KIND_COUNT] = {0};
  for (;;) {
    int8_t bestKind = -1;
    unsigned long bestMs = 0;
    for (uint8_t k = DccDrivable::DCC_MSG_SPEED; k < DccDrivable::DCC_MSG_KIND_COUNT; k++) {
      auto kind = static_cast<DccDrivable::DccMsgKind>(k);
      if (next[k] >= DccDrivable::dccLogSize(kind))
        continue;
      unsigned long ms = DccDrivable::dccLogAt(kind, next[k]).atMs;
      if (bestKind == -1 || ms < bestMs) {
        bestKind = k;
        bestMs = ms;
      }
    }
    if (bestKind == -1)
      break;
    auto kind = static_cast<DccDrivable::DccMsgKind>(bestKind);
    merged[mergedCount++] = &DccDrivable::dccLogAt(kind, next[bestKind]);
    next[bestKind]++;
  }

  JsonArray log = doc[kDccLog].to<JsonArray>();
  for (uint8_t i = 0; i < mergedCount; i++) {
    const DccDrivable::DccLogEntry &e = *merged[i];
    JsonObject o = log.add<JsonObject>();
    for (const auto &k : kKinds) {
      if (k.kind == e.kind) {
        o[kDccLogKind] = k.key;
        break;
      }
    }
    o[kDccLogAddress] = e.address;
    o[kDccLogValue] = e.value;
    o[kDccLastMs] = e.atMs;
    o[kDccLogDevice] = e.deviceName ? String(e.deviceName) : String();
    o[kDccLogRepeat] = e.repeatCount;
  }
  #else
  doc[kDccEnabled] = false;
  #endif

  String json;
  serializeJson(doc, json);
  ApiServer::sendJson(kOk, json);
}

// ---------------------------------------------------------------------------
// Restart
// ---------------------------------------------------------------------------

/** @brief Send a 200 OK response then trigger an immediate ESP32 restart. */
void DeviceApi::_onRestart() {
  LOG_PRINTLN(F("API: POST /api/restart"));
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
  #ifdef LFX_OLED_ENABLED
  OledDisplay::showMessage(LFX_PROJECT_NAME, "Redemarrage...");
  #endif
  delay(400);
  ESP.restart();
}

/**
 * @brief Schedule a hot-reload of the config from LittleFS without rebooting.
 *
 * Returns 200 immediately; the actual reload is deferred to Core 1 and
 * executed by ConfigManager::handlePendingReload() on the next loop() pass.
 * Works regardless of whether devices are currently running.
 */
void DeviceApi::_onReload() {
  LOG_PRINTLN(F("API: POST /api/reload"));
  ConfigManager::requestReload();
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

#endif // LFX_API_SERVER_ENABLED
