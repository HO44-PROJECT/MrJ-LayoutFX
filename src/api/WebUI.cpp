/**
 * @file WebUI.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#ifdef WEBUI
#ifdef ESP32

#include "api/WebUI.h"
#include "api/ApiServer.h"
#include "api/webui_html.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_system.h>     // esp_get_free_heap_size, esp_chip_info
#ifdef SPI_CARDS
#  include "spi/Spi595Bus.h"
#endif

#define FIRMWARE_VERSION "v1"

// ---------------------------------------------------------------------------
// Static member
// ---------------------------------------------------------------------------

const DeviceFactory* WebUI::_factory = nullptr;

// ---------------------------------------------------------------------------
// (HTML page is in src/web/webui.html — gzipped into include/api/webui_html.h
//  by tools/build_webui.py at each build)
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void WebUI::init(const DeviceFactory& factory) {
    _factory = &factory;

    ApiServer::on("/ui",          HTTP_GET,  _onGetUi);
    ApiServer::on("/api/devices", HTTP_GET,  _onGetDevices);
    ApiServer::on("/api/device",  HTTP_POST, _onPostDevice);
    ApiServer::on("/api/all",     HTTP_POST, _onAllDevices);
    ApiServer::on("/api/group",   HTTP_POST, _onGroupDevices);
    ApiServer::on("/api/config",  HTTP_GET,  _onGetConfig);
    ApiServer::on("/api/config",  HTTP_POST, _onPostConfig);
    ApiServer::on("/api/status",  HTTP_GET,  _onGetStatus);
    ApiServer::on("/api/test/gpio", HTTP_POST, _onTestGpio);
    ApiServer::on("/api/test/spi",  HTTP_POST, _onTestSpi);
}

// ---------------------------------------------------------------------------
// Private — HTTP handlers
// ---------------------------------------------------------------------------

void WebUI::_onGetUi() {
    ApiServer::server().sendHeader("Content-Encoding", "gzip");
    ApiServer::server().send_P(200, "text/html", (const char*)WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
}

void WebUI::_onGetDevices() {
    String json = "[";
    for (size_t i = 0; i < _factory->count(); i++) {
        Device*  d     = _factory->device(i);
        uint8_t  board = _factory->deviceBoard(i);
        uint8_t  pin   = 0;
        if (d->getPinCount() > 0) {
#ifdef SPI_CARDS
            pin = d->getPin(0).pin;
#else
            pin = (uint8_t)d->getPin(0);
#endif
        }
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
        json += F(",\"pin\":");
        json += (int)pin;
        json += F("}");
    }
    json += "]";
    ApiServer::server().send(200, "application/json", json);
}

void WebUI::_onPostDevice() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }

    const char* id = doc["id"] | "";
    if (!doc["state"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"state required\"}"));
        return;
    }
    int state = doc["state"].as<int>();

    Serial.print(F("WebUI: device \""));
    Serial.print(id);
    Serial.print(F("\" → state "));
    Serial.println(state);

    for (size_t i = 0; i < _factory->count(); i++) {
        if (strcmp(_factory->deviceId(i), id) == 0) {
            _factory->device(i)->newState((STATE_TYPE)state);
            ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
            return;
        }
    }

    Serial.print(F("WebUI: device not found: "));
    Serial.println(id);
    ApiServer::server().send(404, "application/json", F("{\"error\":\"device not found\"}"));
}

void WebUI::_onAllDevices() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err || !doc["state"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }
    int state = doc["state"].as<int>();

    Serial.print(F("WebUI: all → state "));
    Serial.println(state);

    for (size_t i = 0; i < _factory->count(); i++) {
        Device* d = _factory->device(i);
        if (strcmp("StaticLow", (const char*)d->getDeviceName()) != 0) {
            d->newState((STATE_TYPE)state);
        }
    }
    ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

void WebUI::_onGroupDevices() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err || !doc["state"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }
    int state = doc["state"].as<int>();
    const char* type = doc["type"] | "";

    Serial.print(F("WebUI: group \""));
    Serial.print(type);
    Serial.print(F("\" → state "));
    Serial.println(state);

    for (size_t i = 0; i < _factory->count(); i++) {
        Device* d = _factory->device(i);
        if (strcmp(type, (const char*)d->getDeviceName()) == 0) {
            d->newState((STATE_TYPE)state);
        }
    }
    ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

// ---------------------------------------------------------------------------
// Config file management
// ---------------------------------------------------------------------------

void WebUI::_onGetConfig() {
    if (!LittleFS.exists("/config.json")) {
        ApiServer::server().send(404, "application/json", F("{\"error\":\"config not found\"}"));
        return;
    }
    File f = LittleFS.open("/config.json", "r");
    // Content-Disposition lets the browser save the file with a meaningful name
    ApiServer::server().sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
    ApiServer::server().streamFile(f, "application/json");
    f.close();
}

void WebUI::_onPostConfig() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }
    File f = LittleFS.open("/config.json", "w");
    if (!f) {
        ApiServer::server().send(500, "application/json", F("{\"error\":\"write failed\"}"));
        return;
    }
    f.print(ApiServer::server().arg("plain"));
    f.close();
    ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
    delay(300);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// ESP32 system status
// ---------------------------------------------------------------------------

void WebUI::_onGetStatus() {
    JsonDocument doc;

    // Firmware
    doc["version"]    = FIRMWARE_VERSION;
    doc["build_date"] = __DATE__ " " __TIME__;
    doc["uptime_s"]   = millis() / 1000UL;

    // CPU
    doc["cpu_mhz"]    = ESP.getCpuFreqMHz();
    doc["chip"]       = ESP.getChipModel();
    doc["chip_rev"]   = ESP.getChipRevision();

    // Heap
    doc["heap_free"]  = ESP.getFreeHeap();
    doc["heap_total"] = ESP.getHeapSize();
    doc["heap_min"]   = ESP.getMinFreeHeap();

    // Temperature (internal sensor, rough estimate)
    doc["temp_c"]     = temperatureRead();

    // Sketch / flash
    doc["sketch_size"] = ESP.getSketchSize();
    doc["sketch_free"] = ESP.getFreeSketchSpace();

    // LittleFS
    doc["fs_total"] = LittleFS.totalBytes();
    doc["fs_used"]  = LittleFS.usedBytes();

    String json;
    serializeJson(doc, json);
    ApiServer::server().send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Raw GPIO test (debug view — bypasses device state machine)
// ---------------------------------------------------------------------------

void WebUI::_onTestGpio() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err || !doc["pin"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }
    int pin   = doc["pin"].as<int>();
    int state = doc["state"] | 0;
    if (pin < 0 || pin > 39) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid pin\"}"));
        return;
    }
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state ? HIGH : LOW);
    Serial.print(F("WebUI test: GPIO "));
    Serial.print(pin);
    Serial.print(F(" -> "));
    Serial.println(state);
    ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

// ---------------------------------------------------------------------------
// Raw SPI channel test (debug view — bypasses device state machine)
// ---------------------------------------------------------------------------

void WebUI::_onTestSpi() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err || !doc["card"].is<int>() || !doc["channel"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }
    int card    = doc["card"].as<int>();
    int channel = doc["channel"].as<int>();
    int state   = doc["state"] | 0;
#ifdef SPI_CARDS
    if (!Spi595Bus::ready()) {
        ApiServer::server().send(503, "application/json", F("{\"error\":\"SPI not ready\"}"));
        return;
    }
    Spi595Bus::testPin((uint8_t)card, (uint8_t)channel, (uint8_t)(state ? 1 : 0));
    Serial.print(F("WebUI test: SPI card="));
    Serial.print(card);
    Serial.print(F(" ch="));
    Serial.print(channel);
    Serial.print(F(" -> "));
    Serial.println(state);
    ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
#else
    (void)card; (void)channel;
    ApiServer::server().send(501, "application/json", F("{\"error\":\"SPI_CARDS not enabled\"}"));
#endif
}

#endif  // ESP32
#endif  // WEBUI
