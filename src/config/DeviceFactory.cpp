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

  #include "MrJRailwayFX.h"
  #ifdef MRJFX_SPI_CARDS_ENABLED
    #include "spi/Spi595Bus.h"
  #endif

  #ifdef MRJFX_LOBOT_SERVO_ENABLED
    #include "servo/LobotServo.h"
  #endif

// ---------------------------------------------------------------------------
// Internal helper — map uart bus key to the ESP32 global HardwareSerial.
// Convention: uart bus keys must be uart0, uart1, or uart2.
// ---------------------------------------------------------------------------
static HardwareSerial *serialFromBusKey(const char *key) {
  if (strcmp(key, "uart0") == 0)
    return &Serial;
  if (strcmp(key, "uart1") == 0)
    return &Serial1;
  if (strcmp(key, "uart2") == 0)
    return &Serial2;
  return nullptr;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool DeviceFactory::load(const char *json, const char *boardTypesJson) {
  // Pre-index pin counts from board_types.json (count pins with a wiring field).
  // Stored in parallel arrays to avoid dynamic allocation on embedded targets.
  char _btTypeNames[FACTORY_MAX_BOARD_TYPES][32] = {};
  uint8_t _btPinCounts[FACTORY_MAX_BOARD_TYPES] = {};
  uint8_t _btCount = 0;

  if (boardTypesJson) {
    JsonDocument btDoc;
    if (deserializeJson(btDoc, boardTypesJson) == DeserializationError::Ok) {
      for (JsonPair kv : btDoc.as<JsonObject>()) {
        if (_btCount >= FACTORY_MAX_BOARD_TYPES)
          break;
        strncpy(_btTypeNames[_btCount], kv.key().c_str(), sizeof(_btTypeNames[0]) - 1);
        uint8_t cnt = 0;
        for (JsonObject p : kv.value()["pins"].as<JsonArray>()) {
          if (!p["wiring"].isNull())
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

  // 1. Parse buses — populates _busEntries[], _dccPin, _spiBus, _ports[]
  if (doc["buses"].is<JsonObject>()) {
    _parseBuses(doc["buses"].as<JsonObject>());
  }

  // 2. Parse boards — resolve busType from key, build _spiCards[] for HC595
  if (doc["boards"].is<JsonArray>()) {
    for (JsonObject bd : doc["boards"].as<JsonArray>()) {
      if (_boardCount >= FACTORY_MAX_BOARDS) {
        LOG_PRINTLN(F("DeviceFactory: FACTORY_MAX_BOARDS reached"));
        break;
      }

      BoardCfg &bcfg = _boards_cfg[_boardCount];
      strncpy(bcfg.id, bd["id"] | "", sizeof(bcfg.id) - 1);
      strncpy(bcfg.label, bd["label"] | "", sizeof(bcfg.label) - 1);
      strncpy(bcfg.typeStr, bd["type"] | "", sizeof(bcfg.typeStr) - 1);
      strncpy(bcfg.busKey, bd["bus"] | "", sizeof(bcfg.busKey) - 1);
      bcfg.busType = _resolveBusType(bcfg.busKey);
      bcfg.pinCount = 0;
      bcfg.spiRank = 0;

      // SPI boards: assign daisy-chain rank and register in _spiCards[]
      if (bcfg.busType == BUS_SPI_MASTER) {
        if (_spiCardCount >= FACTORY_MAX_SPI_CARDS) {
          LOG_PRINTLN(F("DeviceFactory: FACTORY_MAX_SPI_CARDS reached"));
          _boardCount++;
          continue;
        }
        // Derive pin count from board_types.json definition; pin_count in JSON overrides.
        uint8_t structural = 0;
        for (uint8_t t = 0; t < _btCount; t++) {
          if (strcmp(_btTypeNames[t], bcfg.typeStr) == 0) {
            structural = _btPinCounts[t];
            break;
          }
        }
        bcfg.pinCount = (uint8_t)(bd["pin_count"] | (int)structural);
        bcfg.spiRank = _spiCardCount + 1;

        _spiCards[_spiCardCount].type = SPI_CARD_HC595;
        _spiCards[_spiCardCount].pinCount = bcfg.pinCount;
        _spiCardCount++;
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

  // 3. Init Spi595Bus once all SPI boards are registered
  #ifdef MRJFX_SPI_CARDS_ENABLED
  if (_spiBus.configured() && _spiCardCount > 0) {
    uint8_t pinCounts[FACTORY_MAX_SPI_CARDS];
    for (uint8_t i = 0; i < _spiCardCount; i++)
      pinCounts[i] = _spiCards[i].pinCount;
    Spi595Bus::init(_spiBus.mosi, _spiBus.sclk, _spiBus.latch, pinCounts, _spiCardCount);
  }
  #endif

  // 4. Parse devices
  for (JsonObject obj : doc["devices"].as<JsonArray>()) {
    if (_count >= FACTORY_MAX_DEVICES) {
      LOG_PRINTLN(F("DeviceFactory: FACTORY_MAX_DEVICES reached"));
      break;
    }
    Device *d = _createDevice(obj);
    if (d) {
      const char *id = obj["id"] | "";
      strncpy(_ids[_count], id, sizeof(_ids[0]) - 1);
      _ids[_count][sizeof(_ids[0]) - 1] = '\0';
      _boards[_count] = _resolveBoardIdx(obj["board"]);
      _devices[_count++] = d;
    }
  }
  return true;
}

void DeviceFactory::initAll() {
  for (size_t i = 0; i < _count; i++)
    _devices[i]->initPins();
}

// ---------------------------------------------------------------------------
// Private — buses
// ---------------------------------------------------------------------------

bool DeviceFactory::_parseBuses(JsonObject buses) {
  for (JsonPair kv : buses) {
    const char *busKey = kv.key().c_str();
    JsonObject bus = kv.value().as<JsonObject>();
    const char *type = bus["type"] | "";

    // Register in bus catalog
    if (_busCount < FACTORY_MAX_BUSES) {
      strncpy(_busEntries[_busCount].key, busKey, sizeof(_busEntries[0].key) - 1);
    }

    if (strcmp(type, "dcc") == 0) {
      _dccPin = bus["pin"] | -1;
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_DCC;
      LOG_PRINT(F("DeviceFactory: bus dcc pin="));
      LOG_PRINTLN(_dccPin);
    } else if (strcmp(type, "spi_master_only") == 0) {
      _spiBus.mosi = bus["mosi"] | -1;
      _spiBus.sclk = bus["sclk"] | -1;
      _spiBus.latch = bus["latch"] | -1;
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_SPI_MASTER;
      if (_spiBus.configured()) {
        LOG_PRINT(F("DeviceFactory: bus spi mosi="));
        LOG_PRINT(_spiBus.mosi);
        LOG_PRINT(F(" sclk="));
        LOG_PRINT(_spiBus.sclk);
        LOG_PRINT(F(" latch="));
        LOG_PRINTLN(_spiBus.latch);
      } else {
        LOG_PRINTLN(F("DeviceFactory: bus spi — incomplete config, ignored"));
      }
    } else if (strcmp(type, "spi_full_duplex") == 0) {
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_SPI_FULL;
      LOG_PRINT(F("DeviceFactory: bus spi_full_duplex "));
      LOG_PRINT(busKey);
      LOG_PRINTLN(F(" — not yet handled"));
    } else if (strcmp(type, "uart") == 0) {
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_UART;
      if (_portCount < FACTORY_MAX_PORTS) {
        PortCfg &cfg = _ports[_portCount];
        strncpy(cfg.name, busKey, sizeof(cfg.name) - 1);
        cfg.name[sizeof(cfg.name) - 1] = '\0';
        cfg.tx = bus["tx"] | -1;
        cfg.rx = bus["rx"] | -1;
        cfg.baud = bus["baud"] | 115200;
        cfg.serial = serialFromBusKey(cfg.name);
        if (cfg.serial && cfg.tx >= 0 && cfg.rx >= 0) {
          cfg.serial->begin(cfg.baud, SERIAL_8N1, cfg.rx, cfg.tx);
          LOG_PRINT(F("DeviceFactory: bus uart "));
          LOG_PRINT(cfg.name);
          LOG_PRINT(F(" tx="));
          LOG_PRINT(cfg.tx);
          LOG_PRINT(F(" rx="));
          LOG_PRINT(cfg.rx);
          LOG_PRINT(F(" baud="));
          LOG_PRINTLN(cfg.baud);
        } else {
          LOG_PRINT(F("DeviceFactory: bus uart "));
          LOG_PRINT(busKey);
          LOG_PRINTLN(F(" — key must be uart0/uart1/uart2"));
        }
        _portCount++;
      } else {
        LOG_PRINTLN(F("DeviceFactory: FACTORY_MAX_PORTS reached"));
      }
    } else if (strcmp(type, "i2c") == 0) {
      if (_busCount < FACTORY_MAX_BUSES)
        _busEntries[_busCount].type = BUS_I2C;
      LOG_PRINT(F("DeviceFactory: bus i2c "));
      LOG_PRINT(busKey);
      LOG_PRINT(F(" sda="));
      LOG_PRINT((int)(bus["sda"] | -1));
      LOG_PRINT(F(" scl="));
      LOG_PRINTLN((int)(bus["scl"] | -1));
    } else {
      LOG_PRINT(F("DeviceFactory: unknown bus type — "));
      LOG_PRINTLN(type);
    }

    if (_busCount < FACTORY_MAX_BUSES)
      _busCount++;
  }
  return true;
}

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

DeviceFactory::PortCfg *DeviceFactory::_findPort(const char *busKey) {
  for (size_t i = 0; i < _portCount; i++) {
    if (strcmp(_ports[i].name, busKey) == 0)
      return &_ports[i];
  }
  return nullptr;
}

HardwareSerial *DeviceFactory::_findSerial(const char *busKey) {
  PortCfg *cfg = _findPort(busKey);
  return cfg ? cfg->serial : nullptr;
}

// ---------------------------------------------------------------------------
// Private — board resolution
// ---------------------------------------------------------------------------

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

uint8_t DeviceFactory::_resolveBoardIdx(JsonVariant v) const {
  if (v.is<const char *>())
    return _resolveBoardId(v.as<const char *>());
  return 0;
}

// ---------------------------------------------------------------------------
// Private — pin helpers
// ---------------------------------------------------------------------------

PIN_ID DeviceFactory::_pin(JsonVariant v, uint8_t boardIdx) {
  uint8_t bit = v.is<JsonArray>()
                    ? (uint8_t)v.as<JsonArray>()[0].as<int>()
                    : (uint8_t)v.as<int>();

  if (boardIdx > 0 && boardIdx <= _boardCount) {
    const BoardCfg &bcfg = _boards_cfg[boardIdx - 1];
    if (bcfg.busType == BUS_SPI_MASTER && bcfg.spiRank > 0) {
  #ifdef MRJFX_SPI_CARDS_ENABLED
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

Device *DeviceFactory::_createDevice(JsonObject obj) {
  const char *type = obj["type"] | "";
  const char *label = obj["label"] | " ";
  int address = obj["address"] | 0;
  JsonVariant wiring = obj["wiring"];
  uint8_t boardIdx = _resolveBoardIdx(obj["board"]);

  Device *d = nullptr;

  // ------------------------------------------------------------------
  // Single-pin LED effects
  // ------------------------------------------------------------------
  if (strcmp(type, "Beacon") == 0)
    d = new Beacon(_pin(wiring, boardIdx));
  else if (strcmp(type, "CampFire") == 0)
    d = new CampFire(_pin(wiring, boardIdx));
  else if (strcmp(type, "Led") == 0)
    d = new Led(_pin(wiring, boardIdx));
  else if (strcmp(type, "DefectLamp") == 0)
    d = new DefectLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, "ElectricLamp") == 0)
    d = new ElectricLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, "GasLamp") == 0)
    d = new GasLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, "NeonSign") == 0)
    d = new NeonSign(_pin(wiring, boardIdx));
  else if (strcmp(type, "OilLamp") == 0)
    d = new OilLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, "SignalFlare") == 0)
    d = new SignalFlare(_pin(wiring, boardIdx));
  else if (strcmp(type, "SolderLamp") == 0)
    d = new SolderLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, "Storm") == 0)
    d = new Storm(_pin(wiring, boardIdx));
  else if (strcmp(type, "Torch") == 0)
    d = new Torch(_pin(wiring, boardIdx));
  else if (strcmp(type, "TrainHeadLamp") == 0)
    d = new TrainHeadLamp(_pin(wiring, boardIdx));
  else if (strcmp(type, "TurnSignal") == 0)
    d = new TurnSignal(_pin(wiring, boardIdx));

  // ------------------------------------------------------------------
  // StaticLow
  // ------------------------------------------------------------------
  else if (strcmp(type, "StaticLow") == 0) {
    PIN_ID pins[FACTORY_MAX_DEVICES];
    size_t n = _pins(wiring, pins, FACTORY_MAX_DEVICES, boardIdx);
    d = new StaticLow(n, pins);
  }

  // ------------------------------------------------------------------
  // Two-pin
  // ------------------------------------------------------------------
  else if (strcmp(type, "DoubleBeacon") == 0) {
    PIN_ID pins[2] = {NO_PIN, NO_PIN};
    _pins(wiring, pins, 2, boardIdx);
    d = new DoubleBeacon(pins[0], pins[1]);
  }
  else if (strcmp(type, "RailwayCrossingLights") == 0) {
    PIN_ID pins[2] = {NO_PIN, NO_PIN};
    _pins(wiring, pins, 2, boardIdx);
    d = new RailwayCrossingLights(pins[0], pins[1]);
  }

  // ------------------------------------------------------------------
  // 2-pin signals
  // ------------------------------------------------------------------
  else if (strcmp(type, "MrJDBBlocSignal") == 0) {
    PIN_ID pins[2] = {NO_PIN, NO_PIN};
    _pins(wiring, pins, 2, boardIdx);
    d = new MrJDBBlocSignal(pins);
  }

  // ------------------------------------------------------------------
  // 3-pin signals
  // ------------------------------------------------------------------
  else if (strcmp(type, "MrJDBEntrySignal") == 0) {
    PIN_ID pins[3] = {NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 3, boardIdx);
    d = new MrJDBEntrySignal(pins);
  } else if (strcmp(type, "TrafficLight3Phase") == 0) {
    PIN_ID pins[3] = {NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 3, boardIdx);
    d = new TrafficLight3Phases(pins);
  } else if (strcmp(type, "TrafficLight4Phase") == 0) {
    PIN_ID pins[3] = {NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 3, boardIdx);
    d = new TrafficLight4Phases(pins);
  }

  // ------------------------------------------------------------------
  // 4-pin signals
  // ------------------------------------------------------------------
  else if (strcmp(type, "MrJDBExitSignal") == 0) {
    PIN_ID pins[4] = {NO_PIN, NO_PIN, NO_PIN, NO_PIN};
    _pins(wiring, pins, 4, boardIdx);
    d = new MrJDBExitSignal(pins);
  }

  // ------------------------------------------------------------------
  // DfAudio — rx/tx come from the board's uart bus
  // ------------------------------------------------------------------
  else if (strcmp(type, "DfAudio") == 0) {
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
  // SerialServo — HardwareSerial comes from the board's uart bus
  // ------------------------------------------------------------------
  else if (strcmp(type, "SerialServo") == 0) {
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
    HardwareSerial *ser = _findSerial(bcfg.busKey);
    if (!ser) {
      LOG_PRINT(F("DeviceFactory: SerialServo — uart not found for bus: "));
      LOG_PRINTLN(bcfg.busKey);
      return nullptr;
    }
    uint8_t servoId = (uint8_t)wiring.as<int>();
    if (_lobotCount >= FACTORY_MAX_DEVICES) {
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
  // Common post-creation setup
  // ------------------------------------------------------------------
  if (label[0] != '\0' && label[0] != ' ')
    d->setLabel(label[0]);
  if (address > 0)
    d->registerDccDrivableDevice((ADDRESS)address);
  if (strcmp(obj["default_state"] | "off", "on") == 0)
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
