/**
 * @file DeviceApi.cpp
 * @brief DeviceApi — route registration and device control endpoints
 *        (/api/devices, /api/device, /api/switch, /api/all, /api/group, /api/servo).
 *
 * Config management → DeviceConfigApi.cpp
 * Status / boards / health → DeviceStatusApi.cpp
 * Hardware diagnostics → DeviceTestApi.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/DeviceApi.h"

#ifdef LFX_API_SERVER_ENABLED

using namespace api_keys;
using namespace http_status;

// ---------------------------------------------------------------------------
// Static member
// ---------------------------------------------------------------------------

const DeviceFactory *DeviceApi::_factory = nullptr;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

/**
 * @brief Register all /api/ routes on ApiServer and store the factory reference.
 * @param factory Read-only reference to the populated DeviceFactory.
 */
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
  ApiServer::on("/api/device-types", HTTP_GET, _onGetDeviceTypes);
  ApiServer::on("/api/bus-types", HTTP_GET, _onGetBusTypes);
  ApiServer::on("/api/i2c-known", HTTP_GET, _onGetI2cKnown);
  ApiServer::on("/api/health", HTTP_GET, _onGetHealth);
  ApiServer::on("/api/dcc-status", HTTP_GET, _onGetDccStatus);
  ApiServer::on("/api/test/gpio", HTTP_POST, _onTestGpio);
  ApiServer::on("/api/test/spi", HTTP_POST, _onTestSpi);
  ApiServer::on("/api/test/identify", HTTP_POST, _onIdentify);
  ApiServer::on("/api/restart", HTTP_POST, _onRestart);
  ApiServer::on("/api/reload",  HTTP_POST, _onReload);
  ApiServer::on("/api/servo", HTTP_POST, _onServo);
  #ifdef LFX_I2C_SCAN_ENABLED
  ApiServer::on("/api/scan/i2c", HTTP_GET, _onScanI2c);
  #endif

  static const char *hdrs[] = {"X-Config-Name"};
  ApiServer::server().collectHeaders(hdrs, 1);
}

// ---------------------------------------------------------------------------
// Device listing
// ---------------------------------------------------------------------------

/**
 * @brief Return a JSON array of all devices with id, type, state, pins, board.
 *        Streamed one device at a time (chunked transfer) instead of building
 *        the whole array in one String — keeps RAM use flat as device count
 *        grows, instead of O(device count) for a single grow-only buffer.
 */
void DeviceApi::_onGetDevices() {
  #ifdef LFX_API_AUDIT_ENABLED
  LOG_PRINTLN(F("API: GET /api/devices"));
  #endif
  WebServer &server = ApiServer::server();
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(kOk, "application/json", "");
  server.sendContent(F("["));
  for (size_t i = 0; i < _factory->count(); i++) {
    Device *d = _factory->device(i);
    uint8_t board = _factory->deviceBoard(i);
    size_t pc = d->getPinCount();

    String json;
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
  #ifdef LFX_SPI_CARDS_ENABLED
      json += (int)d->getPin(j).pin;
  #else
      json += (int)d->getPin(j);
  #endif
    }
    json += F("]");
  #ifdef LFX_SERIAL_SERVO_ENABLED
    if (strcmp_P("SerialServo", (const char *)d->getDeviceName()) == 0) {
      json += F(",\"servoId\":");
      json += (int)static_cast<SerialServoMotor *>(d)->getServoId();
    }
  #endif
  #ifdef LFX_I2C_DEVICES_ENABLED
    if (strcmp_P("PCA9685Servo", (const char *)d->getDeviceName()) == 0) {
      auto *srv = static_cast<I2cPwmServoDevice *>(d);
      json += F(",\"pulse_min_us\":"); json += srv->getPulseMinUs();
      json += F(",\"pulse_max_us\":"); json += srv->getPulseMaxUs();
      json += F(",\"positions\":[");
      for (uint8_t pi = 0; pi < srv->getPosCount(); pi++) {
        if (pi > 0) json += ',';
        json += F("{\"angle\":");
        json += (int)srv->getPosition(pi).angle;
        json += F(",\"duration_ms\":");
        json += (unsigned long)srv->getPosition(pi).duration_ms;
        const char *lbl = srv->getPosition(pi).label;
        if (lbl && *lbl) { json += F(",\"label\":\""); json += lbl; json += '"'; }
        if (srv->getPosition(pi).ease_out) { json += F(",\"ease_out\":true"); }
        json += '}';
      }
      json += ']';
    }
    if (strcmp_P("PCA9685Motor", (const char *)d->getDeviceName()) == 0) {
      auto *mtr = static_cast<I2cPwmMotorDevice *>(d);
      json += F(",\"neutral_us\":"); json += mtr->getNeutralUs();
      json += F(",\"states\":[");
      for (uint8_t si = 0; si < mtr->getMotorStateCount(); si++) {
        if (si > 0) json += ',';
        const I2cPwmMotorDevice::MotorState &ms = mtr->getMotorState(si);
        json += F("{\"speed\":");        json += (int)ms.speed;
        json += F(",\"duration_ms\":"); json += (unsigned long)ms.duration_ms;
        json += F(",\"ramp_up_ms\":"); json += (unsigned long)ms.ramp_up_ms;
        json += F(",\"ramp_down_ms\":"); json += (unsigned long)ms.ramp_down_ms;
        if (ms.label[0]) { json += F(",\"label\":\""); json += ms.label; json += '"'; }
        json += '}';
      }
      json += ']';
    }
  #endif
    json += F("}");
    server.sendContent(json);
  }
  server.sendContent(F("]"));
}

// ---------------------------------------------------------------------------
// Device control
// ---------------------------------------------------------------------------

/**
 * @brief Set the state of one device.
 *        Body: {"id":"<id>","state":<n>}. Returns 404 if id not found.
 */
void DeviceApi::_onPostDevice() {
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) || !doc[kState].is<int>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc[kId] | "";
  int state = doc[kState].as<int>();
  // A cockpit state pick honours the configured startup delay, same as the
  // ALL/group buttons; the Boards tab's hardware test passes skip_delay to
  // get an instant response instead (same convention as /api/all).
  bool skipDelay = doc[kSkipDelay] | false;

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      Device *d = _factory->device(i);
      d->newState((STATE_TYPE)state, skipDelay);
  #ifdef LFX_OLED_ENABLED
      OledDisplay::notify(String(d->getDeviceName()).c_str(), id, state);
  #endif
      ApiServer::sendJson(kOk, F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::sendJson(kNotFound, F("{\"error\":\"device not found\"}"));
}

/**
 * @brief Call switchOn() or switchOff() on one device.
 *        Body: {"id":"<id>","on":<bool>}. Returns 404 if id not found.
 */
void DeviceApi::_onSwitch() {
  #ifdef LFX_API_AUDIT_ENABLED
  LOG_PRINTLN(F("API: POST /api/switch"));
  #endif
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) || !doc[kOn].is<bool>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc[kId] | "";
  bool on = doc[kOn].as<bool>();
  LOG_PRINTF("[switch] id='%s' on=%d\n", id, (int)on);

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      Device *d = _factory->device(i);
      // #8: a single manual click on the device's own icon skips the startup delay.
      if (on)
        d->switchOn(true);
      else
        d->switchOff(true);
  #ifdef LFX_OLED_ENABLED
      OledDisplay::notify(String(d->getDeviceName()).c_str(), id, on ? 1 : 0);
  #endif
      ApiServer::sendJson(kOk, F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::sendJson(kNotFound, F("{\"error\":\"device not found\"}"));
}

/**
 * @brief Set state on all non-static devices, optionally filtered by board.
 *        Body: {"state":<n>[,"board":<1-based-index>]}.
 */
void DeviceApi::_onAllDevices() {
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) || !doc[kState].is<int>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int state = doc[kState].as<int>();
  int board = doc[kBoard] | 0;
  // #8: the Boards tab (hardware wiring test) wants an instant response, not
  // a staggered one — same as a single manual click. The cockpit's ALL
  // ON/OFF buttons omit this field, so they keep honouring the delay.
  bool skipDelay = doc[kSkipDelay] | false;

  for (size_t i = 0; i < _factory->count(); i++) {
    if (board > 0 && _factory->deviceBoard(i) != (uint8_t)board)
      continue;
    Device *d = _factory->device(i);
    if (strcmp("StaticLow", (const char *)d->getDeviceName()) != 0)
      d->newState((STATE_TYPE)state, skipDelay);
  }
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

/**
 * @brief Set state on all devices matching a given type name.
 *        Body: {"type":"<ClassName>","state":<n>}.
 */
void DeviceApi::_onGroupDevices() {
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) || !doc[kState].is<int>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int state = doc[kState].as<int>();
  const char *type = doc[kType] | "";

  for (size_t i = 0; i < _factory->count(); i++) {
    Device *d = _factory->device(i);
    if (strcmp(type, (const char *)d->getDeviceName()) == 0)
      d->newState((STATE_TYPE)state);
  }
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

// ---------------------------------------------------------------------------
// Servo
// ---------------------------------------------------------------------------

/**
 * @brief Set motor speed or reverse direction on a SerialServo device.
 *        Body: {"id":"<id>","speed":<-1000..1000>} or {"id":"<id>","action":"reverse"}.
 */
void DeviceApi::_onServo() {
  #ifdef LFX_API_AUDIT_ENABLED
  LOG_PRINTLN(F("API: POST /api/servo"));
  #endif
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain))) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  const char *id = doc[kId] | "";
  const char *action = doc[kAction] | "";
  bool isReverse = strcmp(action, "reverse") == 0;
  if (!isReverse && !doc[kSpeed].is<int>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"speed or action required\"}"));
    return;
  }

  for (size_t i = 0; i < _factory->count(); i++) {
    if (strcmp(_factory->deviceId(i), id) == 0) {
      Device *d = _factory->device(i);
      if (isReverse)
        d->reverseMotor();
      else
        d->setMotorSpeed((int16_t)doc[kSpeed].as<int>());
  #ifdef LFX_OLED_ENABLED
      // Show the requested action on the OLED, like the switch/device handlers
      // do for the other effects (#63). Reverse uses the -1 sentinel (the UI
      // only sends the non-negative preset speeds); preset speeds map to their
      // labels in OledDisplay::_stateName ("SerialServo" branch).
      OledDisplay::notify(String(d->getDeviceName()).c_str(), id,
                          isReverse ? -1 : doc[kSpeed].as<int>());
  #endif
      ApiServer::sendJson(kOk, F("{\"ok\":true}"));
      return;
    }
  }
  ApiServer::sendJson(kNotFound, F("{\"error\":\"device not found\"}"));
}

#endif // LFX_API_SERVER_ENABLED
