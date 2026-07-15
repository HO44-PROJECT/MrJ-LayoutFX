/**
 * @file DeviceFactory.cpp
 *
 * @brief Implementation of DeviceFactory — maps JSON config → Device instances.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "config/DeviceFactory.h"

#ifdef LFX_CONFIG_ENABLED

  // Device subclass headers — included here only (not in .h) to avoid polluting all consumers.
  #include "devices/StaticLow.h"
  #include "led_fx/Beacon.h"
  #include "led_fx/CampFire.h"
  #include "led_fx/DefectLamp.h"
  #include "led_fx/DoubleBeacon.h"
  #include "led_fx/ElectricLamp.h"
  #include "led_fx/GasLamp.h"
  #include "led_fx/Led.h"
  #include "led_fx/NeonSign.h"
  #include "led_fx/OilLamp.h"
  #include "led_fx/RailwayCrossingLights.h"
  #include "led_fx/SignalFlare.h"
  #include "led_fx/SolderLamp.h"
  #include "led_fx/Storm.h"
  #include "led_fx/Torch.h"
  #include "led_fx/TrainHeadLamp.h"
  #include "led_fx/TurnSignal.h"
  #include "signals/MrJDbBlocSignal.h"
  #include "signals/MrJDbEntrySignal.h"
  #include "signals/MrJDbExitSignal.h"
  #include "traffic/TrafficLight3Phase.h"
  #include "traffic/TrafficLight4Phase.h"
  #ifdef LFX_AUDIO_ENABLED
    #include "audio/DfAudio.h"
  #endif
  #ifdef LFX_SERIAL_SERVO_ENABLED
    #include "servo/SerialServoMotorMode.h"
  #endif
  #ifdef LFX_I2C_DEVICES_ENABLED
    #include "servo/I2cPwmServoDevice.h"
    #include "servo/I2cPwmMotorDevice.h"
  #endif

using namespace factory_keys;

// ---------------------------------------------------------------------------
// UART key → HardwareSerial mapping
// ---------------------------------------------------------------------------

/** @brief Maps a uart bus key to its ESP32 global HardwareSerial instance. */
static HardwareSerial *serialFromBusKey(const char *key) {
  static const struct {
    const char *key;
    HardwareSerial *serial;
  } kUartMap[] = {
      {kUartKey0, &Serial},
      {kUartKey1, &Serial1},
      {kUartKey2, &Serial2},
  };
  for (const auto &e : kUartMap) {
    if (strcmp(key, e.key) == 0)
      return e.serial;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @brief Parse a JSON config document and instantiate all buses, boards and devices.
 *
 * The three parsing passes are ordered intentionally:
 *   1. Buses   — builds the bus catalog (_busEntries[]) and registers each bus in
 *                BusRegistry. Hardware is NOT initialised at this stage.
 *   2. Boards  — resolves each board's bus type and, for SPI boards, assigns a
 *                daisy-chain rank and registers the slot in BusRegistry.
 *   3. Devices — instantiates Device subclasses; bus hardware is activated lazily
 *                here (activateUart/activateSpi) on first device creation.
 *
 * @param json           Null-terminated JSON string (device config).
 * @param boardTypesJson Optional null-terminated JSON string (board_types.json).
 *                       Used to infer SPI board pin counts when "pin_count" is absent.
 * @return true on success, false if the main JSON fails to parse.
 */
bool DeviceFactory::load(const char *json, const BtPinCount *btPinCounts, uint8_t btCount) {
  // SPI boards that omit an explicit "pin_count" derive their size from the
  // caller-supplied structural map (board type → number of wiring pins),
  // generated from board_types.json into embedded_board_pincounts.h.

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    LOG_PRINT(F("DeviceFactory: JSON error — "));
    LOG_PRINTLN(err.c_str());
    return false;
  }

  // Extract config name (for OLED idle screen).
  {
    const char *name = doc[kName] | "";
    strncpy(_configName, name, sizeof(_configName) - 1);
    _configName[sizeof(_configName) - 1] = '\0';
  }

  // Pass 1 — buses. Remember whether a "buses" section exists at all: it lets
  // init() tell "config manages buses, uart0 absent → log off" from "legacy/empty
  // config → keep the compiled LOG_SERIAL default" (see logBusRequest()).
  _busesSection = doc[kSecBuses].is<JsonObject>();
  // Re-derived from THIS config: without this reset the flag sticks to true
  // across hot-reloads once a uart0 bus has been seen, and logBusRequest()
  // keeps answering LOG_BUS_ON after the bus was deleted (#65).
  _uart0LogBus = false;
  if (_busesSection)
    _parseBuses(doc[kSecBuses].as<JsonObject>());

  // Pass 2 — boards.
  if (doc[kSecBoards].is<JsonArray>()) {
    for (JsonObject bd : doc[kSecBoards].as<JsonArray>()) {
      if (_boardCount >= LFX_FACTORY_MAX_BOARDS) {
        LOG_PRINTLN(F("DeviceFactory: LFX_FACTORY_MAX_BOARDS reached"));
        break;
      }

      BoardCfg &bcfg = _boards_cfg[_boardCount];
      strncpy(bcfg.id, bd[kFId] | "", sizeof(bcfg.id) - 1);
      strncpy(bcfg.label, bd[kFLabel] | "", sizeof(bcfg.label) - 1);
      strncpy(bcfg.typeStr, bd[kFType] | "", sizeof(bcfg.typeStr) - 1);
      strncpy(bcfg.busKey, bd[kFBus] | "", sizeof(bcfg.busKey) - 1);
      bcfg.busType = _resolveBusType(bcfg.busKey);
      bcfg.pinCount = 0;
      bcfg.spiRank = 0;

      if (bcfg.busType == BUS_I2C) {
        bcfg.i2cAddress   = (uint8_t)(bd[kFI2cAddress] | 0x40);
        bcfg.oscillatorHz = bd[kFOscillatorHz] | 25000000U;
      }

      if (bcfg.busType == BUS_SPI_MASTER) {
        if (_spiCardCount >= LFX_FACTORY_MAX_SPI_CARDS) {
          LOG_PRINTLN(F("DeviceFactory: LFX_FACTORY_MAX_SPI_CARDS reached"));
          _boardCount++;
          continue;
        }

        // The output count is STRUCTURAL to the board type (embedded_board_pincounts.h,
        // generated from board_types.json). A config "pin_count" is honoured only as a
        // fallback for types unknown to this firmware build — never as an override of a
        // known type: a stale user override once truncated a 16-output card to 8 (#54).
        uint8_t structural = 0;
        for (uint8_t t = 0; btPinCounts && t < btCount; t++) {
          if (strcmp(btPinCounts[t].type, bcfg.typeStr) == 0) {
            structural = btPinCounts[t].pins;
            break;
          }
        }
        bcfg.pinCount = (structural > 0) ? structural : (uint8_t)(bd[kFPinCount] | 0);
        bcfg.spiRank = _spiCardCount + 1; // 1-based daisy-chain rank.
        _spiCardCount++;
        // Feeds the pin-count table consumed by Spi595Bus::init (BusRegistry side).
        BusRegistry::regSpiCard(bcfg.pinCount);
      }

      LOG_PRINT(F("DeviceFactory: board["));
      LOG_PRINT(_boardCount + 1);
      LOG_PRINT(F("] id="));
      LOG_PRINT(bcfg.id);
      LOG_PRINT(F(" type="));
      LOG_PRINT(bcfg.typeStr);
      if (bcfg.busKey[0]) {
        LOG_PRINT(F(" bus="));
        LOG_PRINT(bcfg.busKey);
      }
      if (bcfg.spiRank > 0) {
        LOG_PRINT(F(" rank="));
        LOG_PRINT(bcfg.spiRank);
        LOG_PRINT(F(" pins="));
        LOG_PRINT(bcfg.pinCount);
      }
      LOG_PRINTLN();
      _boardCount++;
    }
  }

  // Activate the SPI bus as soon as at least one SPI card is declared, rather
  // than waiting for the first device — the identify/test-pin endpoints need
  // Spi595Bus::ready() to work on a freshly-added, still-empty SPI card (#115).
  #ifdef LFX_SPI_CARDS_ENABLED
  if (_spiCardCount > 0)
    BusRegistry::activateSpi();
  #endif

  // Pass 3 — devices. Bus hardware is activated lazily on first device creation.
  // Parse idle_pins before iterating devices so _idlePinCount is ready before initAll().
  _idlePinCount = 0;
  if (doc[kSecIdlePins].is<JsonArray>()) {
    for (JsonVariant v : doc[kSecIdlePins].as<JsonArray>()) {
      if (_idlePinCount >= MAX_IDLE_PINS) {
        LOG_PRINTLN(F("DeviceFactory: MAX_IDLE_PINS reached — remaining idle_pins ignored"));
        break;
      }
      _idlePins[_idlePinCount++] = (uint8_t)v.as<int>();
    }
  }

  for (JsonObject obj : doc[kSecDevices].as<JsonArray>()) {
    if (_count >= LFX_FACTORY_MAX_DEVICES) {
      LOG_PRINTLN(F("DeviceFactory: LFX_FACTORY_MAX_DEVICES reached"));
      break;
    }
    Device *d = _createDevice(obj);
    if (d) {
      const char *id = obj[kFId] | "";
      strncpy(_ids[_count], id, sizeof(_ids[0]) - 1);
      _ids[_count][sizeof(_ids[0]) - 1] = '\0';
      _boards[_count] = _resolveBoardIdx(obj[kFBoard]);
      // Store default state from JSON (applied after initPins()). Accept a numeric
      // state value (signals/servos: 0,1,2,…) or the legacy "on"/"off" string.
      JsonVariantConst _ds = obj[kFDefaultState];
      _deviceDefaultStates[_count] = _ds.is<int>()
          ? (uint8_t)_ds.as<int>()
          : ((strcmp(_ds | "", kVOn) == 0) ? 1 : 0);
      _devices[_count++] = d;
    }
  }
  return true;
}

/**
 * @brief Call initPins() on every Device created by load().
 *
 * Separated from load() so the caller can interpose between construction and
 * hardware initialisation (e.g. to apply saved states first).
 */
void DeviceFactory::initAll() {
  for (size_t i = 0; i < _count; i++)
    _devices[i]->initPins();
}

/**
 * @brief Apply default states to all devices.
 *
 * Must be called after initAll() to override the hardcoded OFF_STATE that
 * initPins() sets on all devices.  Reads the stored default states from
 * config["devices"][i]["default_state"] (parsed during load()).
 *
 * IMPORTANT: Always call newState(), even for OFF_STATE, to trigger the
 * coroutine and ensure proper hardware initialization (e.g. motors need to
 * send neutral pulse, even when OFF).
 */
void DeviceFactory::applyDefaultStates() {
  for (size_t i = 0; i < _count; i++) {
    // Always call newState() to trigger coroutine, even for OFF (state 0)
    _devices[i]->newState(_deviceDefaultStates[i]);
  }
}

/**
 * @brief Drive every GPIO listed in config["idle_pins"] to OUTPUT LOW.
 *
 * Must be called after initAll() — device initPins() may also drive some of
 * these pins, and the last write wins.  In practice the lists should be
 * disjoint (WebUI enforces this), but a redundant write is harmless.
 *
 * This silences unassigned output pins that would otherwise float and cause
 * LED flicker via capacitive crosstalk from adjacent active pins.
 */
void DeviceFactory::initIdlePins() {
  for (uint8_t i = 0; i < _idlePinCount; i++) {
    pinMode(_idlePins[i], OUTPUT);
    digitalWrite(_idlePins[i], LOW);
  }
  if (_idlePinCount > 0) {
    LOG_PRINT(F("[Factory] idle_pins: "));
    LOG_PRINT(_idlePinCount);
    LOG_PRINTLN(F(" pin(s) set OUTPUT LOW"));
  }
}

/**
 * @brief Unconditional full reset: detach and delete all running devices, then
 *        clear all bus/board/port state so load() can be called again.
 *
 * Must be called from the Arduino loop() task (Core 1), strictly between two
 * consecutive CoroutineScheduler::loop() calls.  The caller is responsible for
 * calling CoroutineScheduler::setup() and BusRegistry::reset() afterwards.
 */
void DeviceFactory::fullReset() {
  // Stop outputs, detach from AceRoutine scheduler, delete each Device.
  for (size_t i = 0; i < _count; i++) {
    Device *d = _devices[i];
    if (!d) continue;
    d->initPins();                // drive all pins to inactive state
    d->suspend();                 // prevent the scheduler from calling it
    d->detachFromScheduler();     // remove from linked list (safe from Core 1)
    delete d;
    _devices[i] = nullptr;
  }
  _count = 0;

  // Delete PCA9685 I2C drivers (not coroutines, no scheduler involvement).
  #ifdef LFX_I2C_DEVICES_ENABLED
  for (uint8_t i = 0; i < LFX_FACTORY_MAX_BOARDS; i++) {
    if (_pwmDrivers[i]) { delete _pwmDrivers[i]; _pwmDrivers[i] = nullptr; }
  }
  #endif

  // Detach and delete LobotServo coroutines (separate from the Device list).
  #ifdef LFX_LOBOT_SERVO_ENABLED
  for (size_t i = 0; i < _lobotCount; i++) {
    if (_lobotServos[i]) {
      Device::detachCoroutineFromScheduler(_lobotServos[i]);
      delete (LobotServo *)_lobotServos[i];
      _lobotServos[i] = nullptr;
    }
  }
  _lobotCount = 0;
  #endif

  // Reset all config state.
  _boardCount   = 0;
  _busCount     = 0;
  _portCount    = 0;
  _spiCardCount = 0;
  _dccPin       = -1;
  _idlePinCount = 0;
  _spiBus       = SpiBusCfg{};

  for (uint8_t i = 0; i < LFX_FACTORY_MAX_BOARDS; i++)
    _boards_cfg[i] = BoardCfg{};
  for (uint8_t i = 0; i < LFX_FACTORY_MAX_BUSES; i++)
    _busEntries[i] = BusEntry{};
  for (size_t i = 0; i < LFX_FACTORY_MAX_PORTS; i++)
    _ports[i] = PortCfg{};
}

/**
 * @brief Clear all bus/board state so load() can be called again without reboot.
 *
 * Only safe when count() == 0 — no Device objects exist, so no coroutines are
 * running and nothing needs to be stopped or deleted.  This is the case after
 * a first-boot where the config was empty or had only boards but no devices.
 *
 * Does NOT touch hardware (BusRegistry::reset() must be called separately by
 * the caller before the next load()).
 *
 * @return true if the state was cleared, false if devices are already running.
 */
bool DeviceFactory::resetIfEmpty() {
  if (_count > 0) return false;

  _boardCount   = 0;
  _busCount     = 0;
  _portCount    = 0;
  _spiCardCount = 0;
  _dccPin       = -1;
  _idlePinCount = 0;
  _spiBus       = SpiBusCfg{};

  for (uint8_t i = 0; i < LFX_FACTORY_MAX_BOARDS; i++)
    _boards_cfg[i] = BoardCfg{};
  for (uint8_t i = 0; i < LFX_FACTORY_MAX_BUSES; i++)
    _busEntries[i] = BusEntry{};
  for (size_t i = 0; i < LFX_FACTORY_MAX_PORTS; i++)
    _ports[i] = PortCfg{};

  #ifdef LFX_I2C_DEVICES_ENABLED
  for (uint8_t i = 0; i < LFX_FACTORY_MAX_BOARDS; i++)
    _pwmDrivers[i] = nullptr;
  #endif
  #ifdef LFX_LOBOT_SERVO_ENABLED
  _lobotCount = 0;
  #endif

  return true;
}

// ---------------------------------------------------------------------------
// Private — buses
// ---------------------------------------------------------------------------

/**
 * @brief Iterate the "buses" JSON object and register each bus in the internal
 *        catalog (_busEntries[]) and in BusRegistry.
 *
 * Hardware is NOT touched here. BusRegistry::activate*() is called later, on
 * first device creation, so unused buses never open their port.
 *
 * Supported bus types and the fields they consume:
 *   dcc             pin
 *   spi_master_only mosi, sclk, latch
 *   spi_full_duplex (reserved — logged, not handled)
 *   uart            tx, rx, baud
 *   i2c             sda, scl
 */
bool DeviceFactory::_parseBuses(JsonObject buses) {
  for (JsonPair kv : buses) {
    const char *busKey = kv.key().c_str();
    JsonObject bus = kv.value().as<JsonObject>();
    const char *type = bus[kFType] | "";

    // uart0 is the console/log bus, not a device port: its presence keeps Tier-2
    // serial logging on and reserves GPIO1/3. Never register it as a device UART
    // (no device drives the console) — just flag it and move on.
    if (strcmp(busKey, kUartKey0) == 0) {
      _uart0LogBus = true;
      LOG_PRINTLN(F("DeviceFactory: bus uart0 (log) — serial logging ON, GPIO1/3 reserved"));
      continue;
    }

    // Register key in the catalog before branching so it is reachable by
    // _resolveBusType() regardless of which branch runs below.
    if (_busCount < LFX_FACTORY_MAX_BUSES)
      strncpy(_busEntries[_busCount].key, busKey, sizeof(_busEntries[0].key) - 1);

    if (strcmp(type, kBusDcc) == 0) {
      _dccPin = bus[kFPin] | -1;
      if (_busCount < LFX_FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_DCC;
      BusRegistry::regDcc(_dccPin);
      LOG_PRINT(F("DeviceFactory: bus dcc pin="));
      LOG_PRINTLN(_dccPin);

    } else if (strcmp(type, kBusSpiMaster) == 0) {
      _spiBus.mosi = bus[kFMosi] | -1;
      _spiBus.sclk = bus[kFSclk] | -1;
      _spiBus.latch = bus[kFLatch] | -1;
      if (_busCount < LFX_FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_SPI_MASTER;
      if (_spiBus.configured()) {
        BusRegistry::regSpi(_spiBus.mosi, _spiBus.sclk, _spiBus.latch);
        LOG_PRINT(F("DeviceFactory: bus spi mosi="));
        LOG_PRINT(_spiBus.mosi);
        LOG_PRINT(F(" sclk="));
        LOG_PRINT(_spiBus.sclk);
        LOG_PRINT(F(" latch="));
        LOG_PRINTLN(_spiBus.latch);
      } else {
        LOG_PRINTLN(F("DeviceFactory: bus spi — incomplete config (mosi/sclk/latch required), ignored"));
      }

    } else if (strcmp(type, kBusSpiDuplex) == 0) {
      if (_busCount < LFX_FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_SPI_FULL;
      // Full-duplex SPI reserved — no devices use it yet.
      LOG_PRINT(F("DeviceFactory: bus spi_full_duplex "));
      LOG_PRINT(busKey);
      LOG_PRINTLN(F(" — not yet handled"));

    } else if (strcmp(type, kBusUart) == 0) {
      if (_busCount < LFX_FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_UART;
      if (_portCount < LFX_FACTORY_MAX_PORTS) {
        PortCfg &cfg = _ports[_portCount];
        strncpy(cfg.name, busKey, sizeof(cfg.name) - 1);
        cfg.name[sizeof(cfg.name) - 1] = '\0';
        cfg.tx = bus[kFTx] | -1;
        cfg.rx = bus[kFRx] | -1;
        cfg.baud = bus[kFBaud] | 115200;
        cfg.serial = serialFromBusKey(cfg.name); // nullptr if key is not uart0/1/2.
        // Defer serial->begin() — BusRegistry::activateUart() will call it on first use.
        BusRegistry::regUart(cfg.name, cfg.serial, cfg.tx, cfg.rx, cfg.baud);
        LOG_PRINT(F("DeviceFactory: bus uart registered "));
        LOG_PRINT(cfg.name);
        LOG_PRINT(F(" tx="));
        LOG_PRINT(cfg.tx);
        LOG_PRINT(F(" rx="));
        LOG_PRINT(cfg.rx);
        LOG_PRINT(F(" baud="));
        LOG_PRINTLN(cfg.baud);
        _portCount++;
      } else {
        LOG_PRINTLN(F("DeviceFactory: LFX_FACTORY_MAX_PORTS reached"));
      }

    } else if (strcmp(type, kBusI2c) == 0) {
      if (_busCount < LFX_FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_I2C;
      int sda = bus[kFSda] | -1;
      int scl = bus[kFScl] | -1;
      BusRegistry::regI2c(busKey, sda, scl);
      LOG_PRINT(F("DeviceFactory: bus i2c registered "));
      LOG_PRINT(busKey);
      LOG_PRINT(F(" sda="));
      LOG_PRINT(sda);
      LOG_PRINT(F(" scl="));
      LOG_PRINTLN(scl);

    } else {
      LOG_PRINT(F("DeviceFactory: unknown bus type — "));
      LOG_PRINTLN(type);
    }

    if (_busCount < LFX_FACTORY_MAX_BUSES)
      _busCount++;
  }
  return true;
}

/**
 * @brief Look up the BusType for a given bus key.
 *
 * Called during board parsing (pass 2) to resolve each board's bus protocol.
 * Returns BUS_NONE for an empty key (root board) or an unregistered key.
 */
DeviceFactory::BusType DeviceFactory::_resolveBusType(const char *busKey) const {
  if (!busKey || busKey[0] == '\0')
    return BUS_NONE;
  for (uint8_t i = 0; i < _busCount; i++) {
    if (strcmp(_busEntries[i].key, busKey) == 0)
      return _busEntries[i].type;
  }
  return BUS_NONE;
}

// ---------------------------------------------------------------------------
// Private — port lookup
// ---------------------------------------------------------------------------

/**
 * @brief Find the PortCfg for a given uart bus key, or nullptr.
 *
 * Used by DfAudio to retrieve the rx/tx pin numbers without activating the UART.
 * DfAudio uses SoftwareSerial internally and drives the pins itself.
 */
DeviceFactory::PortCfg *DeviceFactory::_findPort(const char *busKey) {
  for (size_t i = 0; i < _portCount; i++) {
    if (strcmp(_ports[i].name, busKey) == 0)
      return &_ports[i];
  }
  return nullptr;
}

/**
 * @brief Return the HardwareSerial* stored in the PortCfg for a bus key, or nullptr.
 *
 * Does NOT activate (begin) the port. Prefer BusRegistry::activateUart() for
 * devices that drive the port themselves (e.g. SerialServo).
 */
HardwareSerial *DeviceFactory::_findSerial(const char *busKey) {
  PortCfg *cfg = _findPort(busKey);
  return cfg ? cfg->serial : nullptr;
}

// ---------------------------------------------------------------------------
// Private — board resolution
// ---------------------------------------------------------------------------

/**
 * @brief Resolve a board id string to a 1-based board index.
 *
 * Returns 0 when the id is empty or not found (0 = "root MCU / no board").
 */
uint8_t DeviceFactory::_resolveBoardId(const char *id) const {
  if (!id || id[0] == '\0')
    return 0;
  for (uint8_t i = 0; i < _boardCount; i++) {
    if (strcmp(_boards_cfg[i].id, id) == 0)
      return i + 1;
  }
  LOG_PRINT(F("DeviceFactory: unknown board id — "));
  LOG_PRINTLN(id);
  return 0;
}

/**
 * @brief Variant of _resolveBoardId() that accepts a JsonVariant.
 *
 * Handles the case where the "board" field is absent or not a string.
 */
uint8_t DeviceFactory::_resolveBoardIdx(JsonVariant v) const {
  if (v.is<const char *>())
    return _resolveBoardId(v.as<const char *>());
  return 0;
}

// ---------------------------------------------------------------------------
// Private — pin helpers
// ---------------------------------------------------------------------------

/**
 * @brief Convert a JSON wiring value to a PIN_ID, routing through SPI or GPIO
 *        depending on the board's bus type.
 *
 * For SPI boards, activateSpi() is called here so the bus is guaranteed to be
 * ready before the first PIN_ID::spi() value is consumed by a Device.
 *
 * @param v        JsonVariant holding an int or a single-element int array.
 * @param boardIdx 1-based index into _boards_cfg (0 = root MCU).
 */
PIN_ID DeviceFactory::_pin(JsonVariant v, uint8_t boardIdx) {
  uint8_t bit = v.is<JsonArray>()
                    ? (uint8_t)v.as<JsonArray>()[0].as<int>()
                    : (uint8_t)v.as<int>();

  if (boardIdx > 0 && boardIdx <= _boardCount) {
    const BoardCfg &bcfg = _boards_cfg[boardIdx - 1];
    if (bcfg.busType == BUS_SPI_MASTER && bcfg.spiRank > 0) {
  #ifdef LFX_SPI_CARDS_ENABLED
      BusRegistry::activateSpi(); // Idempotent — only initialises on first call.
      return PIN_ID::spi(bcfg.spiRank, bit);
  #else
      LOG_PRINTLN(F("DeviceFactory: SPI board requires -DSPI_CARDS — device skipped"));
      return (PIN_ID)255; // NO_PIN
  #endif
    }
  }
  // Belt-and-braces: GPIO 6-11 are the ESP32 SPI-flash pins — never legal as
  // effect outputs. Driving them stalls flash access and trips the watchdog
  // into a boot loop (#68), so refuse them here whatever the config says.
  if (bit >= 6 && bit <= 11) {
    LOG_PRINT(F("DeviceFactory: GPIO "));
    LOG_PRINT(bit);
    LOG_PRINTLN(F(" is an SPI-flash pin — wiring refused"));
    return NO_PIN; // works for both PIN_ID variants (struct constant / scalar define)
  }
  #ifdef LFX_SPI_CARDS_ENABLED
  return PIN_ID::gpio(bit);
  #else
  return (PIN_ID)bit;
  #endif
}

/**
 * @brief Resolve a JSON wiring value that may be a single int or an int array.
 *
 * Each element is converted via _pin(). Stops at maxPins.
 * Returns the number of pins written to @p out.
 */
size_t DeviceFactory::_pins(JsonVariant v, PIN_ID *out, size_t maxPins, uint8_t boardIdx) {
  if (v.is<JsonArray>()) {
    JsonArray arr = v.as<JsonArray>();
    size_t n = min((size_t)arr.size(), maxPins);
    for (size_t i = 0; i < n; i++)
      out[i] = _pin(arr[i], boardIdx);
    return n;
  }
  out[0] = _pin(v, boardIdx);
  return 1;
}

// ---------------------------------------------------------------------------
// Private — device factory
// ---------------------------------------------------------------------------

/**
 * @brief Instantiate one Device from a "devices[]" JSON object.
 *
 * Returns nullptr on any error (unknown type, missing board, missing bus, etc.).
 * On success, also applies label, DCC address, and default_state.
 *
 * Device types by pin count:
 *   single-pin  : Beacon, CampFire, Led, DefectLamp, ElectricLamp, GasLamp,
 *                 NeonSign, OilLamp, SignalFlare, SolderLamp, Storm, Torch,
 *                 TrainHeadLamp, TurnSignal
 *   variable    : StaticLow
 *   two-pin     : DoubleBeacon, RailwayCrossingLights, MrJDBBlocSignal
 *   three-pin   : MrJDBEntrySignal, TrafficLight3Phase, TrafficLight4Phase
 *   four-pin    : MrJDBExitSignal
 *   bus-driven  : DfAudio (uart, SoftwareSerial), SerialServo (uart, HardwareSerial)
 */
Device *DeviceFactory::_createDevice(JsonObject obj) {
  const char *type = obj[kFType] | "";
  const char *label = obj[kFLabel] | " ";
  int address = obj[kFAddress] | 0;
  JsonVariant wiring = obj[kFWiring];
  uint8_t boardIdx = _resolveBoardIdx(obj[kFBoard]);

  // Orphan guard: a board that is NAMED but unknown (e.g. deleted along with
  // its bus) must SKIP the device — never fall back to root GPIO, where a 595
  // card's wiring bits would be reinterpreted as raw pin numbers and can land
  // on the ESP32 SPI-flash pins (GPIO 6-11) → watchdog boot loop (#68).
  // The device stays in the config and revives when its board comes back.
  // boardIdx 0 remains legitimate only for an absent/empty "board" field.
  {
    const char *boardId = obj[kFBoard] | "";
    if (boardId[0] != '\0' && boardIdx == 0) {
      LOG_PRINT(F("DeviceFactory: device '"));
      LOG_PRINT(obj[kFId] | "");
      LOG_PRINT(F("' SKIPPED — unknown board '"));
      LOG_PRINT(boardId);
      LOG_PRINTLN(F("'"));
      return nullptr;
    }
  }

  #ifdef LOG_SERIAL
  // uart0 contention guard (#66): while THIS config keeps the uart0 log bus
  // (pass 1 is already parsed, so logBusRequest() is authoritative here — NOT
  // the runtime g_lfxLogActive, which is only reconciled after load), any
  // device wired to GPIO 1 or 3 on a plain-GPIO board is SKIPPED: an effect on
  // TX0 kills the console and one on RX0 fights the USB bridge electrically.
  // The device stays in the config and revives once the log bus is removed.
  {
    LogBusReq req = logBusRequest();
    bool uart0Owned = (req == LOG_BUS_ON) ||
                      (req == LOG_BUS_DEFAULT && g_lfxLogActive);
    bool rootBoard = (boardIdx == 0) ||
                     (boardIdx <= _boardCount && _boards_cfg[boardIdx - 1].isRoot());
    if (uart0Owned && rootBoard) {
      bool onUart0Pins = false;
      if (wiring.is<JsonArray>()) {
        for (JsonVariant v : wiring.as<JsonArray>()) {
          int p = v.as<int>();
          if (p == 1 || p == 3) onUart0Pins = true;
        }
      } else if (wiring.is<int>()) {
        int p = wiring.as<int>();
        if (p == 1 || p == 3) onUart0Pins = true;
      }
      if (onUart0Pins) {
        LOG_PRINT(F("DeviceFactory: device '"));
        LOG_PRINT(obj[kFId] | "");
        LOG_PRINTLN(F("' SKIPPED — GPIO1/3 reserved by the uart0 log bus"));
        return nullptr;
      }
    }
  }
  #endif

  Device *d = nullptr;

  // ------------------------------------------------------------------
  // Single-pin LED effects
  // ------------------------------------------------------------------
  if (strcmp(type, kDevBeacon) == 0)
    d = new Beacon(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevCampFire) == 0)
    d = new CampFire(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevLed) == 0)
    d = new Led(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevDefectLamp) == 0)
    d = new DefectLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevElectricLamp) == 0)
    d = new ElectricLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevGasLamp) == 0)
    d = new GasLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevNeonSign) == 0)
    d = new NeonSign(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevOilLamp) == 0)
    d = new OilLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevSignalFlare) == 0)
    d = new SignalFlare(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevSolderLamp) == 0)
    d = new SolderLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevStorm) == 0)
    d = new Storm(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevTorch) == 0)
    d = new Torch(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevTrainHeadLamp) == 0)
    d = new TrainHeadLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, kDevTurnSignal) == 0)
    d = new TurnSignal(_pin(wiring, boardIdx));

  // ------------------------------------------------------------------
  // Variable-pin
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevStaticLow) == 0) {
    PIN_ID pins[LFX_FACTORY_MAX_DEVICES];
    size_t n = _pins(wiring, pins, LFX_FACTORY_MAX_DEVICES, boardIdx);
    d = new StaticLow(n, pins);
  }

  // ------------------------------------------------------------------
  // Two-pin
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevDoubleBeacon) == 0) {
    PIN_ID pins[2] = {NO_PIN, NO_PIN};
    _pins(wiring, pins, 2, boardIdx);
    d = new DoubleBeacon(pins[0], pins[1]);
  } else if (strcmp(type, kDevRailwayCrossing) == 0) {
    PIN_ID pins[2] = {NO_PIN, NO_PIN};
    _pins(wiring, pins, 2, boardIdx);
    d = new RailwayCrossingLights(pins[0], pins[1]);
  } else if (strcmp(type, kDevMrJDBBlocSignal) == 0) {
    PIN_ID pins[2] = {NO_PIN, NO_PIN};
    _pins(wiring, pins, 2, boardIdx);
    d = new MrJDBBlocSignal(pins);
  }

  // ------------------------------------------------------------------
  // Three-pin
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevMrJDBEntrySignal) == 0) {
    PIN_ID pins[3] = {NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 3, boardIdx);
    d = new MrJDBEntrySignal(pins);
  } else if (strcmp(type, kDevTrafficLight3) == 0) {
    PIN_ID pins[3] = {NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 3, boardIdx);
    d = new TrafficLight3Phases(pins);
  } else if (strcmp(type, kDevTrafficLight4) == 0) {
    PIN_ID pins[3] = {NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 3, boardIdx);
    d = new TrafficLight4Phases(pins);
  }

  // ------------------------------------------------------------------
  // Four-pin
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevMrJDBExitSignal) == 0) {
    PIN_ID pins[4] = {NO_PIN, NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 4, boardIdx);
    d = new MrJDBExitSignal(pins);
  }

  // ------------------------------------------------------------------
  // DfAudio — uses SoftwareSerial driven by rx/tx from the board's uart bus.
  // The HardwareSerial port is NOT opened here; DfAudio owns its own pins.
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevDfAudio) == 0) {
  #ifdef LFX_AUDIO_ENABLED
    if (boardIdx == 0 || boardIdx > _boardCount) {
      LOG_PRINTLN(F("DeviceFactory: DfAudio — board not found"));
      return nullptr;
    }
    const BoardCfg &bcfg = _boards_cfg[boardIdx - 1];
    if (bcfg.busType != BUS_UART) {
      LOG_PRINT(F("DeviceFactory: DfAudio — board is not on a uart bus: "));
      LOG_PRINTLN(bcfg.id);
      return nullptr;
    }
    PortCfg *cfg = _findPort(bcfg.busKey);
    if (!cfg || cfg->rx < 0 || cfg->tx < 0) {
      LOG_PRINT(F("DeviceFactory: DfAudio — uart config missing for bus: "));
      LOG_PRINTLN(bcfg.busKey);
      return nullptr;
    }
    #ifdef LFX_SPI_CARDS_ENABLED
    d = new DfAudio(PIN_ID::gpio((uint8_t)cfg->rx), PIN_ID::gpio((uint8_t)cfg->tx));
    #else
    d = new DfAudio((PIN_ID)(uint8_t)cfg->rx, (PIN_ID)(uint8_t)cfg->tx);
    #endif
  #endif // LFX_AUDIO_ENABLED
  }

  // ------------------------------------------------------------------
  // SerialServo — drives a Lobot LX-16A chain over HardwareSerial.
  // The port is opened lazily here via BusRegistry::activateUart().
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevSerialServo) == 0) {
  #ifdef LFX_LOBOT_SERVO_ENABLED
    if (boardIdx == 0 || boardIdx > _boardCount) {
      LOG_PRINTLN(F("DeviceFactory: SerialServo — board not found"));
      return nullptr;
    }
    const BoardCfg &bcfg = _boards_cfg[boardIdx - 1];
    if (bcfg.busType != BUS_UART) {
      LOG_PRINT(F("DeviceFactory: SerialServo — board is not on a uart bus: "));
      LOG_PRINTLN(bcfg.id);
      return nullptr;
    }
    HardwareSerial *ser = BusRegistry::activateUart(bcfg.busKey);
    if (!ser) {
      LOG_PRINT(F("DeviceFactory: SerialServo — failed to activate uart: "));
      LOG_PRINTLN(bcfg.busKey);
      return nullptr;
    }
    uint8_t servoId = (uint8_t)wiring.as<int>();
    if (_lobotCount >= LFX_FACTORY_MAX_DEVICES) {
      LOG_PRINTLN(F("DeviceFactory: LOBOT servo array full"));
      return nullptr;
    }
    LobotServo *ls = new LobotServo(*ser, servoId);
    _lobotServos[_lobotCount++] = ls;
    d = new SerialServoMotor(ls, NO_PIN, NO_PIN, (int)servoId);
  #else
    LOG_PRINTLN(F("DeviceFactory: SerialServo requires build_flags = -DLOBOT"));
    return nullptr;
  #endif
  }

  // ------------------------------------------------------------------
  // PCA9685Servo — multi-position slewing servo on a PCA9685 I2C board.
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevI2cPwmServo) == 0) {
  #ifdef LFX_I2C_DEVICES_ENABLED
    if (boardIdx == 0 || boardIdx > _boardCount) {
      LOG_PRINTLN(F("DeviceFactory: PCA9685Servo — board not found"));
      return nullptr;
    }
    const BoardCfg &bcfg = _boards_cfg[boardIdx - 1];
    if (bcfg.busType != BUS_I2C) {
      LOG_PRINT(F("DeviceFactory: PCA9685Servo — board is not on an i2c bus: "));
      LOG_PRINTLN(bcfg.id);
      return nullptr;
    }
    if (!_pwmDrivers[boardIdx - 1]) {
      BusRegistry::activateI2c();
      _pwmDrivers[boardIdx - 1] = new Adafruit_PWMServoDriver(bcfg.i2cAddress);
      _pwmDrivers[boardIdx - 1]->begin();
      // Silence all 16 channels BEFORE setPWMFreq(50) so that the RESTART
      // sequence inside setPWMFreq finds FULL_OFF values and outputs nothing.
      // (begin() leaves channels at ON=0,OFF=0 which is unpredictable per
      // the PCA9685 datasheet and can produce a high signal at 50 Hz.)
      for (uint8_t ch = 0; ch < 16; ch++)
        _pwmDrivers[boardIdx - 1]->setPWM(ch, 0, 4096);
      if (bcfg.oscillatorHz != 25000000U)
        _pwmDrivers[boardIdx - 1]->setOscillatorFrequency(bcfg.oscillatorHz);
      _pwmDrivers[boardIdx - 1]->setPWMFreq(50);
    }
    {
      uint8_t channel = (uint8_t)wiring.as<int>();
      I2cPwmServoDevice::Position pos[I2cPwmServoDevice::MAX_POSITIONS];
      uint8_t posCount = 0;
      for (JsonObject p : obj[kFPositions].as<JsonArray>()) {
        if (posCount >= I2cPwmServoDevice::MAX_POSITIONS) break;
        pos[posCount].angle       = (int16_t)(p[kFAngle]      | 90);
        pos[posCount].duration_ms = (uint32_t)(p[kFDurationMs] | 2000);
        pos[posCount].ease_out    = (bool)(p[kFEaseOut]    | false);
        const char *lbl = p[kFLabel] | "";
        strncpy(pos[posCount].label, lbl, sizeof(pos[posCount].label) - 1);
        pos[posCount].label[sizeof(pos[posCount].label) - 1] = '\0';
        posCount++;
      }
      uint16_t pMin = (uint16_t)(obj[kFPulseMinUs] | 1000);
      uint16_t pMax = (uint16_t)(obj[kFPulseMaxUs] | 2000);
      d = new I2cPwmServoDevice(_pwmDrivers[boardIdx - 1], channel, pos, posCount, pMin, pMax);
    }
  #else
    LOG_PRINTLN(F("DeviceFactory: PCA9685Servo requires build_flags = -DI2C_CARDS"));
    return nullptr;
  #endif
  }

  // ------------------------------------------------------------------
  // PCA9685Motor — continuous-rotation motor on a PCA9685 I2C board.
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevI2cPwmMotor) == 0) {
  #ifdef LFX_I2C_DEVICES_ENABLED
    if (boardIdx == 0 || boardIdx > _boardCount) {
      LOG_PRINTLN(F("DeviceFactory: PCA9685Motor — board not found"));
      return nullptr;
    }
    const BoardCfg &bcfg = _boards_cfg[boardIdx - 1];
    if (bcfg.busType != BUS_I2C) {
      LOG_PRINT(F("DeviceFactory: PCA9685Motor — board is not on an i2c bus: "));
      LOG_PRINTLN(bcfg.id);
      return nullptr;
    }
    if (!_pwmDrivers[boardIdx - 1]) {
      BusRegistry::activateI2c();
      _pwmDrivers[boardIdx - 1] = new Adafruit_PWMServoDriver(bcfg.i2cAddress);
      _pwmDrivers[boardIdx - 1]->begin();
      // Same pre-setPWMFreq silencing as for PCA9685Servo — see comment above.
      for (uint8_t ch = 0; ch < 16; ch++)
        _pwmDrivers[boardIdx - 1]->setPWM(ch, 0, 4096);
      if (bcfg.oscillatorHz != 25000000U)
        _pwmDrivers[boardIdx - 1]->setOscillatorFrequency(bcfg.oscillatorHz);
      _pwmDrivers[boardIdx - 1]->setPWMFreq(50);
    }
    {
      uint8_t  channel   = (uint8_t)wiring.as<int>();
      uint16_t neutralUs = (uint16_t)(obj[kFNeutralUs] | 1500);

      I2cPwmMotorDevice::MotorState states[I2cPwmMotorDevice::MAX_STATES];
      uint8_t stateCount = 0;

      if (obj[kFStates].is<JsonArray>()) {
        for (JsonObject s : obj[kFStates].as<JsonArray>()) {
          if (stateCount >= I2cPwmMotorDevice::MAX_STATES) break;
          states[stateCount].speed        = (int8_t)(s[kFSpeed]     | 50);
          states[stateCount].duration_ms  = (uint32_t)(s[kFDurationMs] | 0);
          states[stateCount].ramp_up_ms   = (uint32_t)(s[kFRampUpMs]   | 0);
          states[stateCount].ramp_down_ms = (uint32_t)(s[kFRampDownMs] | 0);
          const char *lbl = s[kFLabel] | "";
          strncpy(states[stateCount].label, lbl, sizeof(states[stateCount].label) - 1);
          states[stateCount].label[sizeof(states[stateCount].label) - 1] = '\0';
          stateCount++;
        }
      } else {
        // Backward compat: old "speed" field → single perpetual state, no ramps.
        states[0].speed        = (int8_t)(obj[kFSpeed] | 50);
        states[0].duration_ms  = 0;
        states[0].ramp_up_ms   = 0;
        states[0].ramp_down_ms = 0;
        states[0].label[0]     = '\0';
        stateCount = 1;
      }
      d = new I2cPwmMotorDevice(_pwmDrivers[boardIdx - 1], channel, states, stateCount, neutralUs);
    }
  #else
    LOG_PRINTLN(F("DeviceFactory: PCA9685Motor requires build_flags = -DI2C_CARDS"));
    return nullptr;
  #endif
  }

  else {
    LOG_PRINT(F("DeviceFactory: unknown type — "));
    LOG_PRINTLN(type);
    return nullptr;
  }

  // ------------------------------------------------------------------
  // Common post-creation setup (label, DCC address, default state).
  // ------------------------------------------------------------------
  if (label[0] != '\0' && label[0] != ' ')
    d->setLabel(label[0]);
  if (address > 0)
    d->registerDccDrivableDevice((ADDRESS)address);

  // Default state will be applied after initPins() via applyDefaultStates().
  // Do NOT call newState() here — initPins() hasn't been called yet.

  LOG_PRINT(F("DeviceFactory: created "));
  LOG_PRINT(type);
  LOG_PRINT(F(" label="));
  LOG_PRINT(label[0]);
  LOG_PRINT(F(" addr="));
  LOG_PRINTLN(address);

  return d;
}

#endif // LFX_CONFIG_ENABLED
