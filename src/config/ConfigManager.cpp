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
    // Boot-critical: always print directly to Serial, regardless of LOG_SERIAL.
    Serial.println(F("[FS] ERROR: mount failed — check partition scheme (Tools > Partition Scheme)"));
    return;
  }

  // Check for required files before any open() to suppress noisy vfs_api errors.
  // Boot-critical messages go directly to Serial, not through LOG_PRINTLN,
  // so they are always visible even when LOG_SERIAL is not defined.
  bool boardTypesMissing = !LittleFS.exists("/board_types.json");
  bool configMissing     = !LittleFS.exists(_configPath);

  if (boardTypesMissing || configMissing) {
    Serial.println(F("[FS] WARNING: filesystem is empty or incomplete."));
    Serial.println(F("[FS]   -> In PlatformIO: run 'Upload Filesystem Image' (littlefs) to upload the data/ folder."));
    if (boardTypesMissing) Serial.println(F("[FS]   missing: board_types.json"));
    if (configMissing)     Serial.println(F("[FS]   missing: config.json"));
  }

  if (configMissing) {
    Serial.println(F("[Factory] no config — skipping device load"));
    return;
  }

  String boardTypes = _readFile("/board_types.json");
  String json = readConfig();
  if (!json.isEmpty()) {
    LOG_PRINTLN(F("[Factory] loading config..."));
    if (_factory.load(json.c_str(), boardTypes.isEmpty() ? nullptr : boardTypes.c_str())) {
      _factory.initAll();
      LOG_PRINT(F("[Factory] "));
      LOG_PRINT(_factory.count());
      LOG_PRINTLN(F(" device(s) ready"));
      if (_factory.dccPin() >= 0) {
        DccDrivable::init((uint8_t)_factory.dccPin());
      }
    } else {
      LOG_PRINTLN(F("[Factory] JSON parse error"));
    }
  } else {
    LOG_PRINTLN(F("[Factory] no config"));
  }
}

String ConfigManager::readConfig() {
  return _readFile(_configPath);
}

bool ConfigManager::writeConfig(const String &json) {
  File f = LittleFS.open(_configPath, "w", true); // create=true required on arduino-esp32 3.x
  if (!f)
    return false;
  const uint8_t *buf = (const uint8_t *)json.c_str();
  size_t total = json.length();
  size_t offset = 0;
  const size_t CHUNK = 512;
  while (offset < total) {
    size_t toWrite = min(total - offset, CHUNK);
    size_t w = f.write(buf + offset, toWrite);
    if (w == 0) { f.close(); return false; }
    offset += w;
  }
  f.close();
  return true;
}

void ConfigManager::deleteConfig() {
  LittleFS.remove(_configPath);
}

bool ConfigManager::configExists() {
  return LittleFS.exists(_configPath);
}

String ConfigManager::listConfigs() {
  String out = "[";
  bool first = true;
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    String name = f.name();
    if (name.startsWith("/")) name = name.substring(1); // strip leading slash (ESP32 LittleFS quirk)
    if (name.endsWith(".json") && name != "board_types.json") {
      if (!first) out += ",";
      out += "\"";
      out += name;
      out += "\"";
      first = false;
    }
    f = root.openNextFile();
  }
  out += "]";
  return out;
}

bool ConfigManager::activateConfig(const char *srcFile) {
  String content = _readFile(srcFile);
  if (content.isEmpty()) return false;
  if (!writeConfig(content)) return false;
  // Remember which source file is active
  File f = LittleFS.open("/config_source.txt", "w", true); // create=true required on arduino-esp32 3.x
  if (f) { f.print(srcFile); f.close(); }
  return true;
}

// ---------------------------------------------------------------------------
// LittleFS helpers
// ---------------------------------------------------------------------------

String ConfigManager::readFile(const char *path) {
  return _readFile(path);
}

String ConfigManager::_readFile(const char *path) {
  // Check existence before open() to avoid noisy vfs_api "does not exist" errors in Serial log.
  if (!LittleFS.exists(path))
    return String();
  File f = LittleFS.open(path, "r");
  if (!f)
    return String();
  String s = f.readString();
  f.close();
  return s;
}

#endif // MRJFX_CONFIG_ENABLED
