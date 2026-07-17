/**
 * @file ConfigManager.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "config/ConfigManager.h"

#ifdef LFX_CONFIG_ENABLED

  #ifdef LFX_OLED_ENABLED
    #include "oled/OledDisplay.h"
  #endif

  // Structural pin-count map (board type → pin count) for SPI cards, generated
  // from board_types.json. Passed to DeviceFactory::load() so SPI boards that omit
  // "pin_count" are sized correctly. (board_types.json itself is gzipped in PROGMEM.)
  #include "generated/embedded_board_pincounts.h"

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
  // NOTE: board_types.json is now embedded in firmware (PROGMEM), not in LittleFS.
  bool configMissing = !LittleFS.exists(_configPath);

  if (configMissing) {
    Serial.println(F("[FS] WARNING: filesystem is empty or incomplete."));
    Serial.println(F("[FS]   -> In PlatformIO: run 'Upload Filesystem Image' (littlefs) to upload the data/ folder."));
    Serial.println(F("[FS]   missing: config.json"));
    Serial.println(F("[Factory] no config — skipping device load"));
    return;
  }

  // board_types.json is embedded in firmware (PROGMEM) and served via API; the
  // SPI pin counts it implies are passed to load() via BOARD_PIN_COUNTS so SPI
  // cards are sized even when a board config omits "pin_count".
  String json = readConfig();
  if (!json.isEmpty()) {
    LOG_PRINTLN(F("[Factory] loading config..."));
    if (_factory.load(json.c_str(), BOARD_PIN_COUNTS, BOARD_PIN_COUNTS_LEN)) {
  #ifdef LOG_SERIAL
      // Close UART0 HERE — after load() (bus/device parsing) but BEFORE initAll().
      // load() only PARSES the config: no pin is touched yet, so the console is
      // still safe to use for the messages above and below. initAll() is what
      // calls initPins() on every device, which for a device wired to GPIO1/3
      // (legal once the log bus is off — see the #66 guard in DeviceFactory)
      // immediately drives that pin as an output. If UART0 were still open at
      // that point, the electrical contention between the live console and the
      // freshly-driven output pin corrupts whatever is mid-transmission on the
      // wire — this is what truncated the boot log mid-line in practice.
      // Losing the boot IP from serial in this case is an acceptable trade — it
      // is still shown on the OLED and via /api/status once WiFi is up.
      {
        DeviceFactory::LogBusReq req = _factory.logBusRequest();
        g_lfxLogActive = (req == DeviceFactory::LOG_BUS_ON)  ? true
                         : (req == DeviceFactory::LOG_BUS_OFF) ? false
                         : g_lfxLogActive; // DEFAULT (no "buses" section) → compiled value
        if (!g_lfxLogActive) {
          Serial.println(F("[Log] uart0 bus off — releasing GPIO1/3, serial console now silent"));
          Serial.flush();
          Serial.end();
        }
      }
  #endif
      _factory.initAll();
      _factory.applyDefaultStates();
      _factory.initIdlePins();
  #ifdef LFX_OLED_ENABLED
      OledDisplay::setConfigName(_factory.configName());
      {
        uint8_t h = 64;
        bool present = _factory.findOledBoard(h);
        OledDisplay::configure(present, h);
      }
  #endif
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
  // Atomic write: fill a temp file, then swap it in. A power cut mid-write can only
  // corrupt the .tmp — the live config is replaced in one rename, never truncated.
  String tmp = String(_configPath) + ".tmp";
  File f = LittleFS.open(tmp.c_str(), "w", true); // create=true required on arduino-esp32 3.x
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
      LittleFS.remove(tmp.c_str()); // failed write → drop the temp, keep the old config
      return false;
    }
    offset += w;
  }
  f.close();
  // Swap the temp over the live config; on failure keep the existing one intact.
  LittleFS.remove(_configPath);
  if (!LittleFS.rename(tmp.c_str(), _configPath)) {
    LittleFS.remove(tmp.c_str());
    return false;
  }
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

  String json = readConfig();
  if (json.isEmpty()) {
    LOG_PRINTLN(F("[Factory] reload — no config"));
    return false;
  }

  LOG_PRINTLN(F("[Factory] reloading config..."));
  if (!_factory.load(json.c_str(), BOARD_PIN_COUNTS, BOARD_PIN_COUNTS_LEN)) {
    LOG_PRINTLN(F("[Factory] reload — JSON parse error"));
    return false;
  }

  #ifdef LOG_SERIAL
  // Close UART0 BEFORE initAll() if the reloaded config turns the log bus off —
  // see the identical comment in ConfigManager::init() for why this ordering
  // (not after initAll()) is required to avoid corrupting in-flight console
  // output when a device newly wired to GPIO1/3 gets driven as an output.
  {
    DeviceFactory::LogBusReq req = _factory.logBusRequest();
    if (req == DeviceFactory::LOG_BUS_OFF && g_lfxLogActive) {
      Serial.println(F("[Log] uart0 bus off — releasing GPIO1/3, serial console now silent"));
      Serial.flush();
      Serial.end();
      g_lfxLogActive = false;
    }
  }
  #endif

  _factory.initAll();
  _factory.applyDefaultStates();

  #ifdef LFX_OLED_ENABLED
  {
    uint8_t h = 64;
    bool present = _factory.findOledBoard(h);
    OledDisplay::configure(present, h);
  }
  #endif

  #ifdef LOG_SERIAL
  {
    DeviceFactory::LogBusReq req = _factory.logBusRequest();
    if (req == DeviceFactory::LOG_BUS_ON && !g_lfxLogActive) {
      Serial.begin(115200);
      delay(50);
      g_lfxLogActive = true;
      Serial.println(F("[Log] uart0 bus re-added — serial console reopened, GPIO1/3 reserved"));
    }
  }
  #endif

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
  if (!_reloadPending)
    return;
  _reloadPending = false;

  // Block the HTTP handlers (Core 0) while devices are torn down + rebuilt, so a
  // concurrent request can never dereference a just-deleted Device (backlog #27).
  LFX_DEVICE_LOCK();
  LOG_PRINTLN(F("[Factory] hot-reload..."));
  _factory.fullReset();
  BusRegistry::reset();

  String json = readConfig();
  if (json.isEmpty()) {
    LOG_PRINTLN(F("[Factory] hot-reload — no config"));
    ace_routine::CoroutineScheduler::setup();
    return;
  }

  if (!_factory.load(json.c_str(), BOARD_PIN_COUNTS, BOARD_PIN_COUNTS_LEN)) {
    LOG_PRINTLN(F("[Factory] hot-reload — JSON parse error"));
    ace_routine::CoroutineScheduler::setup();
    return;
  }

  #ifdef LOG_SERIAL
  // Reconcile the uart0 log bus with the reloaded config BEFORE initAll(): removing
  // the bus must close UART0 here, not after, because initAll() (below) calls
  // initPins() on every device — including one now legally wired to GPIO1/3 (the
  // #66 guard only skips it while the bus is active) — which drives that pin as an
  // output immediately. Leaving UART0 open across that call means the console is
  // still transmitting while the pin gets toggled: electrical contention that
  // corrupts/truncates whatever is mid-transmission (the boot-time equivalent of
  // this bit us via a truncated log line). Re-adding the bus is the opposite
  // direction — no pin is ever driven while it's active, so reopening it after
  // initAll() is safe and keeps the "device(s) ready" summary on the console.
  {
    DeviceFactory::LogBusReq req = _factory.logBusRequest();
    if (req == DeviceFactory::LOG_BUS_OFF && g_lfxLogActive) {
      // Direct Serial (Tier-1): last message before the console closes.
      Serial.println(F("[Log] uart0 bus removed — releasing GPIO1/3, serial console now silent"));
      Serial.flush();
      Serial.end();
      g_lfxLogActive = false;
    }
  }
  #endif

  _factory.initAll();
  _factory.applyDefaultStates();
  ace_routine::CoroutineScheduler::setup();

  #ifdef LFX_OLED_ENABLED
  {
    uint8_t h = 64;
    bool present = _factory.findOledBoard(h);
    OledDisplay::configure(present, h);
  }
  #endif

  #ifdef LOG_SERIAL
  {
    DeviceFactory::LogBusReq req = _factory.logBusRequest();
    if (req == DeviceFactory::LOG_BUS_ON && !g_lfxLogActive) {
      Serial.begin(115200); // same fixed baud as the boot path (LayoutFX::init)
      delay(50);            // let the UART/USB bridge settle or the first line is lost
                            // (one-shot inside the reload — not a coroutine path)
      g_lfxLogActive = true;
      Serial.println(F("[Log] uart0 bus re-added — serial console reopened, GPIO1/3 reserved"));
    }
  }
  #endif

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

#endif // LFX_CONFIG_ENABLED
