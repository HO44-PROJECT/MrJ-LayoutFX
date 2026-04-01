/**
 * @file ConfigManager.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#ifdef CONFIG
#ifdef ESP32

#include "config/ConfigManager.h"
#include "api/ApiServer.h"
#include "dcc/DccDrivable.h"
#include <LittleFS.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

DeviceFactory ConfigManager::_factory;
const char*   ConfigManager::_configPath = nullptr;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ConfigManager::init(const char* configPath) {
    _configPath = configPath;

    // --- LittleFS ---
    if (!LittleFS.begin(true)) {
        Serial.println(F("[FS] mount failed"));
    } else {
        Serial.println(F("[FS] mounted"));
    }

    // --- DeviceFactory — init pins before WiFi delay ---
    String boardTypes = _readFile("/board_types.json");
    String json = _readConfig();
    if (!json.isEmpty()) {
        Serial.println(F("[Factory] loading config..."));
        if (_factory.load(json.c_str(), boardTypes.isEmpty() ? nullptr : boardTypes.c_str())) {
            _factory.initAll();
            Serial.print(F("[Factory] "));
            Serial.print(_factory.count());
            Serial.println(F(" device(s) ready"));
            if (_factory.dccPin() >= 0) {
                DccDrivable::init((uint8_t)_factory.dccPin());
            }
        } else {
            Serial.println(F("[Factory] JSON parse error"));
        }
    } else {
        Serial.println(F("[Factory] no config — POST /config to upload one"));
    }

    // --- Register HTTP endpoints on ApiServer ---
    ApiServer::on("/config", HTTP_GET,    _onGetConfig);
    ApiServer::on("/config", HTTP_POST,   _onPostConfig);
    ApiServer::on("/config", HTTP_DELETE, _onDeleteConfig);
    ApiServer::on("/status", HTTP_GET,    _onGetStatus);
}

// ---------------------------------------------------------------------------
// Private — LittleFS helpers
// ---------------------------------------------------------------------------

String ConfigManager::_readFile(const char* path) {
    File f = LittleFS.open(path, "r");
    if (!f) return String();
    String s = f.readString();
    f.close();
    return s;
}

String ConfigManager::_readConfig() {
    return _readFile(_configPath);
}

bool ConfigManager::_writeConfig(const String& json) {
    File f = LittleFS.open(_configPath, "w");
    if (!f) return false;
    f.print(json);
    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// Private — HTTP handlers
// ---------------------------------------------------------------------------

void ConfigManager::_onGetConfig() {
    String json = _readConfig();
    if (json.isEmpty()) {
        ApiServer::server().send(404, "application/json", F("{\"error\":\"no config on device\"}"));
    } else {
        ApiServer::server().send(200, "application/json", json);
    }
}

void ConfigManager::_onPostConfig() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "text/plain", F("body required"));
        return;
    }
    if (_writeConfig(ApiServer::server().arg("plain"))) {
        ApiServer::server().send(200, "text/plain", F("ok — restarting in 300 ms"));
        delay(300);
        ESP.restart();
    } else {
        ApiServer::server().send(500, "text/plain", F("LittleFS write failed"));
    }
}

void ConfigManager::_onDeleteConfig() {
    LittleFS.remove(_configPath);
    ApiServer::server().send(200, "text/plain", F("config deleted — restarting"));
    delay(200);
    ESP.restart();
}

void ConfigManager::_onGetStatus() {
    String s = F("{\"ip\":\"");
    s += WiFi.localIP().toString();
    s += F("\",\"devices\":");
    s += _factory.count();
    s += F(",\"config\":");
    s += LittleFS.exists(_configPath) ? "true" : "false";
    s += F("}");
    ApiServer::server().send(200, "application/json", s);
}

#endif  // ESP32
#endif  // CONFIG
