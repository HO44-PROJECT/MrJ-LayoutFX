/**
 * @file DeviceTestApi.cpp
 * @brief DeviceApi — raw hardware diagnostic endpoints (/api/test/gpio,
 *        /api/test/spi, /api/scan/i2c).
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/DeviceApi.h"

#ifdef MRJFX_API_SERVER_ENABLED

using namespace api_keys;
using namespace http_status;

// ---------------------------------------------------------------------------
// GPIO test
// ---------------------------------------------------------------------------

/**
 * @brief Write a HIGH or LOW level to any GPIO pin not reserved by the firmware.
 *        Body: {"pin":<0..39>,"state":<0|1>}.
 *        Returns 403 for UART0 pins (TX=1, RX=3) and 400 for out-of-range pins.
 */
void DeviceApi::_onTestGpio() {
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) || !doc[kPin].is<int>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int pin = doc[kPin].as<int>();
  int state = doc[kState] | 0;
  if (pin < 0 || pin > kGpioPinMax) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid pin\"}"));
    return;
  }
  if (pin == kUart0TxPin || pin == kUart0RxPin) {
    ApiServer::sendJson(kForbidden, F("{\"error\":\"reserved UART pin\"}"));
    return;
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, state ? HIGH : LOW);
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

// ---------------------------------------------------------------------------
// SPI test
// ---------------------------------------------------------------------------

/**
 * @brief Set one output channel on a 74HC595 SPI card.
 *        Body: {"card":<1-based>,"channel":<1-based>,"state":<0|1>}.
 *        Returns 503 if SPI bus is not initialised, 501 if SPI_CARDS disabled.
 */
void DeviceApi::_onTestSpi() {
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) ||
      !doc[kCard].is<int>() || !doc[kChannel].is<int>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  int card = doc[kCard].as<int>();
  int channel = doc[kChannel].as<int>();
  int state = doc[kState] | 0;
  #ifdef MRJFX_SPI_CARDS_ENABLED
  if (!Spi595Bus::ready()) {
    ApiServer::sendJson(kServiceUnavailable, F("{\"error\":\"SPI not ready\"}"));
    return;
  }
  Spi595Bus::setPin((uint8_t)card, (uint8_t)channel, (uint8_t)(state ? 1 : 0));
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
  #else
  (void)card;
  (void)channel;
  (void)state;
  ApiServer::sendJson(kNotImplemented, F("{\"error\":\"SPI not enabled\"}"));
  #endif
}

// ---------------------------------------------------------------------------
// I2C scanner
// ---------------------------------------------------------------------------

  #ifdef MRJFX_I2C_SCAN_ENABLED
/**
 * @brief Scan all 7-bit I2C addresses and return those that ACK.
 *        Uses I2C_SDA/I2C_SCL (set in config.h or defaulted in MrJRailwayFX_default.h).
 *        Bus is already initialised by MrJFX::init() via MRJFX_I2C_CARDS_ENABLED.
 *        Response: {"sda":<n>,"scl":<n>,"count":<n>,"found":[addr,…]}.
 */
void DeviceApi::_onScanI2c() {
  LOG_PRINTLN(F("API: GET /api/scan/i2c"));
  const int sda = I2C_SDA, scl = I2C_SCL;

  JsonDocument doc;
  doc[kSda] = sda;
  doc[kScl] = scl;
  JsonArray found = doc[kFound].to<JsonArray>();
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0)
      found.add(addr);
  }
  doc[kCount] = found.size();

  if (found.size() > 0 && LittleFS.exists(kPathI2cKnown)) {
    File f = LittleFS.open(kPathI2cKnown, "r");
    JsonDocument known;
    if (!deserializeJson(known, f)) {
      JsonObject names = doc[F("names")].to<JsonObject>();
      for (JsonVariantConst v : found) {
        String key = String(v.as<int>());
        JsonVariantConst n = known[key];
        if (!n.isNull()) names[key] = n;
      }
    }
    f.close();
  }

  String json;
  serializeJson(doc, json);
  ApiServer::sendJson(kOk, json);
}
  #endif

#endif // MRJFX_API_SERVER_ENABLED
