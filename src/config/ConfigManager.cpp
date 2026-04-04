/**
 * @file ConfigManager.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "config/ConfigManager.h"

#ifdef MRJFX_CONFIG_ENABLED

  #include "dcc/DccDrivable.h"
  #include <LittleFS.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

DeviceFactory ConfigManager::_factory;
const char *ConfigManager::_configPath = nullptr;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ConfigManager::init(const char *configPath) {
  _configPath = configPath;

  if (!LittleFS.begin(true)) {
    Serial.println(F("[FS] mount failed"));
  } else {
    Serial.println(F("[FS] mounted"));
  }

  String boardTypes = _readFile("/board_types.json");
  String json = readConfig();
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
    Serial.println(F("[Factory] no config"));
  }
}

String ConfigManager::readConfig() {
  return _readFile(_configPath);
}

bool ConfigManager::writeConfig(const String &json) {
  File f = LittleFS.open(_configPath, "w");
  if (!f)
    return false;
  f.print(json);
  f.close();
  return true;
}

void ConfigManager::deleteConfig() {
  LittleFS.remove(_configPath);
}

bool ConfigManager::configExists() {
  return LittleFS.exists(_configPath);
}

// ---------------------------------------------------------------------------
// Private — LittleFS helpers
// ---------------------------------------------------------------------------

String ConfigManager::_readFile(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f)
    return String();
  String s = f.readString();
  f.close();
  return s;
}

#endif // MRJFX_CONFIG_ENABLED
