/**
 * @file DeviceFactory.cpp
 *
 * @brief Implementation of DeviceFactory — maps JSON config → Device instances.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "config/DeviceFactory.h"

#ifdef MRJFX_CONFIG_ENABLED

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
  #ifdef MRJFX_AUDIO_ENABLED
    #include "audio/DfAudio.h"
  #endif
  #ifdef MRJFX_SERIAL_SERVO_ENABLED
    #include "servo/SerialServoMotorMode.h"
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
bool DeviceFactory::load(const char *json, const char *boardTypesJson) {
  // Pre-index pin counts from board_types.json (count pins with a wiring field).
  // Stored in parallel arrays to avoid dynamic allocation on embedded targets.
  char _btTypeNames[FACTORY_MAX_BOARD_TYPES][FACTORY_ID_LEN] = {};
  uint8_t _btPinCounts[FACTORY_MAX_BOARD_TYPES] = {};
  uint8_t _btCount = 0;

  if (boardTypesJson) {
    JsonDocument btDoc;
    if (deserializeJson(btDoc, boardTypesJson) == DeserializationError::Ok) {
      for (JsonPair kv : btDoc.as<JsonObject>()) {
        if (_btCount >= FACTORY_MAX_BOARD_TYPES)
          break;
        strncpy(_btTypeNames[_btCount], kv.key().c_str(), FACTORY_ID_LEN - 1);
        uint8_t cnt = 0;
        for (JsonObject p : kv.value()[kSecPins].as<JsonArray>()) {
          if (!p[kFWiring].isNull())
            cnt++;
        }
        _btPinCounts[_btCount] = cnt;
        _btCount++;
      }
    }
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    LOG_PRINT(F("DeviceFactory: JSON error — "));
    LOG_PRINTLN(err.c_str());
    return false;
  }

  // Pass 1 — buses.
  if (doc[kSecBuses].is<JsonObject>())
    _parseBuses(doc[kSecBuses].as<JsonObject>());

  // Pass 2 — boards.
  if (doc[kSecBoards].is<JsonArray>()) {
    for (JsonObject bd : doc[kSecBoards].as<JsonArray>()) {
      if (_boardCount >= FACTORY_MAX_BOARDS) {
        LOG_PRINTLN(F("DeviceFactory: FACTORY_MAX_BOARDS reached"));
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

      if (bcfg.busType == BUS_SPI_MASTER) {
        if (_spiCardCount >= FACTORY_MAX_SPI_CARDS) {
          LOG_PRINTLN(F("DeviceFactory: FACTORY_MAX_SPI_CARDS reached"));
          _boardCount++;
          continue;
        }

        // Derive pin count from board_types.json; an explicit kFPinCount overrides.
        uint8_t structural = 0;
        for (uint8_t t = 0; t < _btCount; t++) {
          if (strcmp(_btTypeNames[t], bcfg.typeStr) == 0) {
            structural = _btPinCounts[t];
            break;
          }
        }
        bcfg.pinCount = (uint8_t)(bd[kFPinCount] | (int)structural);
        bcfg.spiRank = _spiCardCount + 1; // 1-based daisy-chain rank.

        _spiCards[_spiCardCount].type = SPI_CARD_HC595;
        _spiCards[_spiCardCount].pinCount = bcfg.pinCount;
        _spiCardCount++;
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

  // Pass 3 — devices. Bus hardware is activated lazily on first device creation.
  for (JsonObject obj : doc[kSecDevices].as<JsonArray>()) {
    if (_count >= MRJFX_FACTORY_MAX_DEVICES) {
      LOG_PRINTLN(F("DeviceFactory: MRJFX_FACTORY_MAX_DEVICES reached"));
      break;
    }
    Device *d = _createDevice(obj);
    if (d) {
      const char *id = obj[kFId] | "";
      strncpy(_ids[_count], id, sizeof(_ids[0]) - 1);
      _ids[_count][sizeof(_ids[0]) - 1] = '\0';
      _boards[_count] = _resolveBoardIdx(obj[kFBoard]);
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

    // Register key in the catalog before branching so it is reachable by
    // _resolveBusType() regardless of which branch runs below.
    if (_busCount < FACTORY_MAX_BUSES)
      strncpy(_busEntries[_busCount].key, busKey, sizeof(_busEntries[0].key) - 1);

    if (strcmp(type, kBusDcc) == 0) {
      _dccPin = bus[kFPin] | -1;
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_DCC;
      BusRegistry::regDcc(_dccPin);
      LOG_PRINT(F("DeviceFactory: bus dcc pin="));
      LOG_PRINTLN(_dccPin);

    } else if (strcmp(type, kBusSpiMaster) == 0) {
      _spiBus.mosi = bus[kFMosi] | -1;
      _spiBus.sclk = bus[kFSclk] | -1;
      _spiBus.latch = bus[kFLatch] | -1;
      if (_busCount < FACTORY_MAX_BUSES)
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
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_SPI_FULL;
      // Full-duplex SPI reserved — no devices use it yet.
      LOG_PRINT(F("DeviceFactory: bus spi_full_duplex "));
      LOG_PRINT(busKey);
      LOG_PRINTLN(F(" — not yet handled"));

    } else if (strcmp(type, kBusUart) == 0) {
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_UART;
      if (_portCount < FACTORY_MAX_PORTS) {
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
        LOG_PRINTLN(F("DeviceFactory: FACTORY_MAX_PORTS reached"));
      }

    } else if (strcmp(type, kBusI2c) == 0) {
      if (_busCount < FACTORY_MAX_BUSES)
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

    if (_busCount < FACTORY_MAX_BUSES)
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
  #ifdef MRJFX_SPI_CARDS_ENABLED
      BusRegistry::activateSpi(); // Idempotent — only initialises on first call.
      return PIN_ID::spi(bcfg.spiRank, bit);
  #else
      LOG_PRINTLN(F("DeviceFactory: SPI board requires -DSPI_CARDS — device skipped"));
      return (PIN_ID)255; // NO_PIN
  #endif
    }
  }
  #ifdef MRJFX_SPI_CARDS_ENABLED
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
    PIN_ID pins[MRJFX_FACTORY_MAX_DEVICES];
    size_t n = _pins(wiring, pins, MRJFX_FACTORY_MAX_DEVICES, boardIdx);
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
  #ifdef MRJFX_AUDIO_ENABLED
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
    #ifdef MRJFX_SPI_CARDS_ENABLED
    d = new DfAudio(PIN_ID::gpio((uint8_t)cfg->rx), PIN_ID::gpio((uint8_t)cfg->tx));
    #else
    d = new DfAudio((PIN_ID)(uint8_t)cfg->rx, (PIN_ID)(uint8_t)cfg->tx);
    #endif
  #endif // MRJFX_AUDIO_ENABLED
  }

  // ------------------------------------------------------------------
  // SerialServo — drives a Lobot LX-16A chain over HardwareSerial.
  // The port is opened lazily here via BusRegistry::activateUart().
  // ------------------------------------------------------------------
  else if (strcmp(type, kDevSerialServo) == 0) {
  #ifdef MRJFX_LOBOT_SERVO_ENABLED
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
    if (_lobotCount >= MRJFX_FACTORY_MAX_DEVICES) {
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
  if (strcmp(obj[kFDefaultState] | "", kVOn) == 0)
    d->newState(1);

  LOG_PRINT(F("DeviceFactory: created "));
  LOG_PRINT(type);
  LOG_PRINT(F(" label="));
  LOG_PRINT(label[0]);
  LOG_PRINT(F(" addr="));
  LOG_PRINTLN(address);

  return d;
}

#endif // MRJFX_CONFIG_ENABLED
