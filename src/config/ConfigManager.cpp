/**
 * @file ConfigManager.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "config/ConfigManager.h"

#ifdef MRJFX_CONFIG_ENABLED

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

DeviceFactory ConfigManager::_factory;
const char *ConfigManager::_configPath = nullptr;
volatile bool ConfigManager::_reloadPending = false;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

/**
 * @brief Mount LittleFS, load and parse config.json, initialise all devices and DCC.
 *        Boot-critical messages go directly to Serial regardless of LOG_SERIAL so they
 *        are always visible when the filesystem is missing or incomplete.
 * @param configPath LittleFS path to the JSON config file (e.g. "/config.json").
 */
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
  bool configMissing = !LittleFS.exists(_configPath);

  if (boardTypesMissing || configMissing) {
    Serial.println(F("[FS] WARNING: filesystem is empty or incomplete."));
    Serial.println(F("[FS]   -> In PlatformIO: run 'Upload Filesystem Image' (littlefs) to upload the data/ folder."));
    if (boardTypesMissing)
      Serial.println(F("[FS]   missing: board_types.json"));
    if (configMissing)
      Serial.println(F("[FS]   missing: config.json"));
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

/** @brief Read the active config file from LittleFS. Returns an empty String if absent. */
String ConfigManager::readConfig() {
  return _readFile(_configPath);
}

/**
 * @brief Write (overwrite) the active config file on LittleFS in kFsChunkSize-byte chunks.
 * @param json JSON string to persist.
 * @return true on success, false if the file could not be opened or a write stalled.
 */
bool ConfigManager::writeConfig(const String &json) {
  File f = LittleFS.open(_configPath, "w", true); // create=true required on arduino-esp32 3.x
  if (!f)
    return false;
  const uint8_t *buf = (const uint8_t *)json.c_str();
  size_t total = json.length();
  size_t offset = 0;
  while (offset < total) {
    size_t toWrite = min(total - offset, kFsChunkSize);
    size_t w = f.write(buf + offset, toWrite);
    if (w == 0) {
      f.close();
      return false;
    }
    offset += w;
  }
  f.close();
  return true;
}

/**
 * @brief Reload config from LittleFS without rebooting.
 *
 * Only safe when no devices are running (factory.count() == 0).
 * Resets bus registry and factory bus/board state, then re-parses the saved
 * config and re-initialises all devices and DCC.
 */
bool ConfigManager::reload() {
  if (!_factory.resetIfEmpty()) {
    LOG_PRINTLN(F("[Factory] reload refused — devices are already running"));
    return false;
  }
  BusRegistry::reset();

  String boardTypes = _readFile("/board_types.json");
  String json = readConfig();
  if (json.isEmpty()) {
    LOG_PRINTLN(F("[Factory] reload — no config"));
    return false;
  }

  LOG_PRINTLN(F("[Factory] reloading config..."));
  if (!_factory.load(json.c_str(), boardTypes.isEmpty() ? nullptr : boardTypes.c_str())) {
    LOG_PRINTLN(F("[Factory] reload — JSON parse error"));
    return false;
  }
  _factory.initAll();
  LOG_PRINT(F("[Factory] reload — "));
  LOG_PRINT(_factory.count());
  LOG_PRINTLN(F(" device(s) ready"));

  if (_factory.dccPin() >= 0)
    DccDrivable::init((uint8_t)_factory.dccPin());

  return true;
}

/** @brief Signal from Core 0 that a hot-reload is needed. */
void ConfigManager::requestReload() {
  _reloadPending = true;
}

/**
 * @brief Execute a pending hot-reload (Core 1, between scheduler passes).
 *
 * Tears down all running devices via fullReset(), resets the AceRoutine
 * scheduler and bus registry, then loads the saved config from LittleFS and
 * starts all new devices.  No ESP.restart() — the system continues running.
 */
void ConfigManager::handlePendingReload() {
  if (!_reloadPending) return;
  _reloadPending = false;

  LOG_PRINTLN(F("[Factory] hot-reload..."));
  _factory.fullReset();
  BusRegistry::reset();

  String boardTypes = _readFile("/board_types.json");
  String json = readConfig();
  if (json.isEmpty()) {
    LOG_PRINTLN(F("[Factory] hot-reload — no config"));
    ace_routine::CoroutineScheduler::setup();
    return;
  }

  if (!_factory.load(json.c_str(), boardTypes.isEmpty() ? nullptr : boardTypes.c_str())) {
    LOG_PRINTLN(F("[Factory] hot-reload — JSON parse error"));
    ace_routine::CoroutineScheduler::setup();
    return;
  }
  _factory.initAll();
  ace_routine::CoroutineScheduler::setup();
  LOG_PRINT(F("[Factory] hot-reload — "));
  LOG_PRINT(_factory.count());
  LOG_PRINTLN(F(" device(s) ready"));

  if (_factory.dccPin() >= 0)
    DccDrivable::init((uint8_t)_factory.dccPin());
}

/** @brief Delete the active config file from LittleFS. Does nothing if absent. */
void ConfigManager::deleteConfig() {
  LittleFS.remove(_configPath);
}

/** @brief Return true if the active config file exists on LittleFS. */
bool ConfigManager::configExists() {
  return LittleFS.exists(_configPath);
}

/**
 * @brief Build a JSON array string of all *.json files in LittleFS root,
 *        excluding system definition files (board_types, device_types, bus_types, i2c_known).
 * @return JSON array string, e.g. ["config.json","backup.json"].
 */
String ConfigManager::listConfigs() {
  String out = "[";
  bool first = true;
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    String name = f.name();
    if (name.startsWith("/"))
      name = name.substring(1); // strip leading slash (ESP32 LittleFS quirk)
    if (name.endsWith(".json") && name != "board_types.json" && name != "device_types.json" && name != "bus_types.json" && name != "i2c_known.json") {
      if (!first)
        out += ",";
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

/**
 * @brief Copy srcFile to configPath, making it the active config.
 *        Also records the source filename in /config_source.txt for the UI.
 * @param srcFile LittleFS path of the source file (e.g. "/config_backup.json").
 * @return true on success, false if the source is empty or the write fails.
 */
bool ConfigManager::activateConfig(const char *srcFile) {
  String content = _readFile(srcFile);
  if (content.isEmpty())
    return false;
  if (!writeConfig(content))
    return false;
  // Remember which source file is active
  File f = LittleFS.open("/config_source.txt", "w", true); // create=true required on arduino-esp32 3.x
  if (f) {
    f.print(srcFile);
    f.close();
  }
  return true;
}

// ---------------------------------------------------------------------------
// LittleFS helpers
// ---------------------------------------------------------------------------

/**
 * @brief Public wrapper — read any file from LittleFS by path.
 * @param path Absolute LittleFS path (e.g. "/board_types.json").
 * @return File contents as a String, or an empty String if absent.
 */
String ConfigManager::readFile(const char *path) {
  return _readFile(path);
}

/**
 * @brief Read a LittleFS file into a String.
 *        Checks existence before open() to suppress noisy vfs_api "does not exist" logs.
 * @param path Absolute LittleFS path.
 * @return File contents, or empty String if the file does not exist or cannot be opened.
 */
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
