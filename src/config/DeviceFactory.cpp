/**
 * @file DeviceFactory.cpp
 *
 * @brief Implementation of DeviceFactory — maps JSON config → Device instances.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#ifdef CONFIG

#ifdef ESP32

#include "config/DeviceFactory.h"
#include "MrJRailwayFX.h"        // pulls in every device class

#ifdef LOBOT
#include "servo/LobotServo.h"
#endif

// ---------------------------------------------------------------------------
// Internal helper — map port name to the ESP32 global HardwareSerial object.
// ---------------------------------------------------------------------------
static HardwareSerial* serialFromPortName(const char* name) {
  if (strcmp(name, "uart0") == 0) return &Serial;
  if (strcmp(name, "uart1") == 0) return &Serial1;
  if (strcmp(name, "uart2") == 0) return &Serial2;
  return nullptr;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool DeviceFactory::load(const char* json) {
  // 2 kB covers ≈ 16 devices with typical field lengths.
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print(F("DeviceFactory: JSON error — "));
    Serial.println(err.c_str());
    return false;
  }

  _dccPin = doc["system"]["dcc_pin"] | -1;

  if (doc["serial_ports"].is<JsonObject>()) {
    _parsePorts(doc["serial_ports"].as<JsonObject>());
  }

  JsonArray arr = doc["devices"].as<JsonArray>();
  for (JsonObject obj : arr) {
    if (_count >= FACTORY_MAX_DEVICES) {
      Serial.println(F("DeviceFactory: FACTORY_MAX_DEVICES reached"));
      break;
    }
    Device* d = _createDevice(obj);
    if (d) {
      _devices[_count++] = d;
    }
  }
  return true;
}

void DeviceFactory::initAll() {
  for (size_t i = 0; i < _count; i++) {
    _devices[i]->initPins();
  }
}

// ---------------------------------------------------------------------------
// Private — serial ports
// ---------------------------------------------------------------------------

bool DeviceFactory::_parsePorts(JsonObject ports) {
  for (JsonPair kv : ports) {
    if (_portCount >= FACTORY_MAX_PORTS) break;

    PortCfg& cfg = _ports[_portCount];
    strncpy(cfg.name, kv.key().c_str(), sizeof(cfg.name) - 1);
    cfg.name[sizeof(cfg.name) - 1] = '\0';

    JsonObject p = kv.value().as<JsonObject>();
    cfg.tx   = p["tx"]   | -1;
    cfg.rx   = p["rx"]   | -1;
    cfg.baud = p["baud"] | 115200;

    cfg.serial = serialFromPortName(cfg.name);
    if (cfg.serial && cfg.tx >= 0 && cfg.rx >= 0) {
      cfg.serial->begin(cfg.baud, SERIAL_8N1, cfg.rx, cfg.tx);
      Serial.print(F("DeviceFactory: opened "));
      Serial.print(cfg.name);
      Serial.print(F(" tx="));  Serial.print(cfg.tx);
      Serial.print(F(" rx="));  Serial.print(cfg.rx);
      Serial.print(F(" baud=")); Serial.println(cfg.baud);
    }
    _portCount++;
  }
  return true;
}

DeviceFactory::PortCfg* DeviceFactory::_findPort(const char* portName) {
  for (size_t i = 0; i < _portCount; i++) {
    if (strcmp(_ports[i].name, portName) == 0) return &_ports[i];
  }
  return nullptr;
}

HardwareSerial* DeviceFactory::_findSerial(const char* portName) {
  PortCfg* cfg = _findPort(portName);
  return cfg ? cfg->serial : nullptr;
}

// ---------------------------------------------------------------------------
// Private — pin helpers
// ---------------------------------------------------------------------------

PIN_ID DeviceFactory::_pin(JsonVariant v) {
  if (v.is<JsonArray>()) return (PIN_ID)v.as<JsonArray>()[0].as<int>();
  return (PIN_ID)v.as<int>();
}

size_t DeviceFactory::_pins(JsonVariant v, PIN_ID* out, size_t maxPins) {
  if (v.is<JsonArray>()) {
    JsonArray arr = v.as<JsonArray>();
    size_t n = min((size_t)arr.size(), maxPins);
    for (size_t i = 0; i < n; i++) out[i] = (PIN_ID)arr[i].as<int>();
    return n;
  }
  out[0] = (PIN_ID)v.as<int>();
  return 1;
}

// ---------------------------------------------------------------------------
// Private — device factory
// ---------------------------------------------------------------------------

Device* DeviceFactory::_createDevice(JsonObject obj) {
  const char* type    = obj["type"]    | "";
  const char* label   = obj["label"]   | " ";
  int         address = obj["address"] | 0;
  const char* port    = obj["port"]    | "";
  JsonVariant wiring  = obj["wiring"];

  Device* d = nullptr;

  // ------------------------------------------------------------------
  // Single-pin LED effects  (wiring: scalar GPIO)
  // ------------------------------------------------------------------
  if      (strcmp(type, "Beacon")                == 0) d = new Beacon               (_pin(wiring));
  else if (strcmp(type, "CampFire")              == 0) d = new CampFire              (_pin(wiring));
  else if (strcmp(type, "DefectLamp")            == 0) d = new DefectLamp            (_pin(wiring));
  else if (strcmp(type, "ElectricLamp")          == 0) d = new ElectricLamp          (_pin(wiring));
  else if (strcmp(type, "GasLamp")               == 0) d = new GasLamp               (_pin(wiring));
  else if (strcmp(type, "NeonSign")              == 0) d = new NeonSign              (_pin(wiring));
  else if (strcmp(type, "OilLamp")               == 0) d = new OilLamp               (_pin(wiring));
  else if (strcmp(type, "RailwayCrossingLights") == 0) d = new RailwayCrossingLights (_pin(wiring));
  else if (strcmp(type, "SignalFlare")           == 0) d = new SignalFlare           (_pin(wiring));
  else if (strcmp(type, "SolderLamp")            == 0) d = new SolderLamp            (_pin(wiring));
  else if (strcmp(type, "Storm")                 == 0) d = new Storm                 (_pin(wiring));
  else if (strcmp(type, "Torch")                 == 0) d = new Torch                 (_pin(wiring));
  else if (strcmp(type, "TrainHeadLamp")         == 0) d = new TrainHeadLamp         (_pin(wiring));
  else if (strcmp(type, "TurnSignal")            == 0) d = new TurnSignal            (_pin(wiring));

  // ------------------------------------------------------------------
  // StaticLow — drive 1-4 pins OUTPUT LOW (wiring: scalar or [gpio…])
  // Useful to suppress boot pull-ups on JTAG/strapping pins with a LED.
  // ------------------------------------------------------------------
  else if (strcmp(type, "StaticLow") == 0) {
    PIN_ID pins[FACTORY_MAX_DEVICES];
    size_t n = _pins(wiring, pins, FACTORY_MAX_DEVICES);
    d = new StaticLow(n, pins);
  }

  // ------------------------------------------------------------------
  // Two-pin alternating effect  (wiring: [gpio, gpio])
  // ------------------------------------------------------------------
  else if (strcmp(type, "DoubleBeacon") == 0) {
    PIN_ID pins[2] = { NO_PIN, NO_PIN };
    _pins(wiring, pins, 2);
    d = new DoubleBeacon(pins[0], pins[1]);
  }

  // ------------------------------------------------------------------
  // 2-pin CharliePlexing signal  (wiring: [gpio, gpio])
  // ------------------------------------------------------------------
  else if (strcmp(type, "MrJDBBlocSignal") == 0) {
    PIN_ID pins[2] = { NO_PIN, NO_PIN };
    _pins(wiring, pins, 2);
    d = new MrJDBBlocSignal(pins);
  }

  // ------------------------------------------------------------------
  // 3-pin CharliePlexing signals  (wiring: [gpio, gpio, gpio])
  // ------------------------------------------------------------------
  else if (strcmp(type, "MrJDBEntrySignal") == 0) {
    PIN_ID pins[3] = { NO_PIN, NO_PIN, NO_PIN };
    _pins(wiring, pins, 3);
    d = new MrJDBEntrySignal(pins);
  }
  else if (strcmp(type, "MrJDBExitSignal") == 0) {
    PIN_ID pins[3] = { NO_PIN, NO_PIN, NO_PIN };
    _pins(wiring, pins, 3);
    d = new MrJDBExitSignal(pins);
  }
  else if (strcmp(type, "TrafficLight3Phase") == 0) {
    PIN_ID pins[3] = { NO_PIN, NO_PIN, NO_PIN };
    _pins(wiring, pins, 3);
    d = new TrafficLight3Phases(pins);
  }

  // ------------------------------------------------------------------
  // 4-pin CharliePlexing signal  (wiring: [gpio, gpio, gpio, gpio])
  // ------------------------------------------------------------------
  else if (strcmp(type, "TrafficLight4Phase") == 0) {
    PIN_ID pins[4] = { NO_PIN, NO_PIN, NO_PIN, NO_PIN };
    _pins(wiring, pins, 4);
    d = new TrafficLight4Phases(pins);
  }

  // ------------------------------------------------------------------
  // DfAudio  (port → rx/tx GPIO; no wiring)
  // ------------------------------------------------------------------
  else if (strcmp(type, "DfAudio") == 0) {
    PortCfg* cfg = _findPort(port);
    if (!cfg || cfg->rx < 0 || cfg->tx < 0) {
      Serial.print(F("DeviceFactory: DfAudio — port not found or incomplete: "));
      Serial.println(port);
      return nullptr;
    }
    d = new DfAudio((PIN_ID)cfg->rx, (PIN_ID)cfg->tx);
  }

  // ------------------------------------------------------------------
  // SerialServo  (port → HardwareSerial; wiring → servo bus ID)
  // ------------------------------------------------------------------
  else if (strcmp(type, "SerialServo") == 0) {
#ifdef LOBOT
    HardwareSerial* ser = _findSerial(port);
    if (!ser) {
      Serial.print(F("DeviceFactory: SerialServo — port not found: "));
      Serial.println(port);
      return nullptr;
    }
    uint8_t servoId = (uint8_t)wiring.as<int>();
    if (_lobotCount >= FACTORY_MAX_DEVICES) {
      Serial.println(F("DeviceFactory: LOBOT servo array full"));
      return nullptr;
    }
    LobotServo* ls = new LobotServo(*ser, servoId);
    _lobotServos[_lobotCount++] = ls;
    d = new SerialServoMotor(ls, NO_PIN, NO_PIN, (int)servoId);
#else
    Serial.println(F("DeviceFactory: SerialServo requires build_flags = -DLOBOT"));
    return nullptr;
#endif
  }

  // ------------------------------------------------------------------
  // Unknown type
  // ------------------------------------------------------------------
  else {
    Serial.print(F("DeviceFactory: unknown type — "));
    Serial.println(type);
    return nullptr;
  }

  // ------------------------------------------------------------------
  // Common post-creation setup
  // ------------------------------------------------------------------
  if (label[0] != '\0' && label[0] != ' ') {
    d->setLabel(label[0]);
  }
  if (address > 0) {
    d->registerDccDrivableDevice((ADDRESS)address);
  }

  const char* defaultState = obj["default_state"] | "off";
  if (strcmp(defaultState, "on") == 0) {
    d->newState(1);
  }

  Serial.print(F("DeviceFactory: created "));
  Serial.print(type);
  Serial.print(F(" label="));  Serial.print(label[0]);
  Serial.print(F(" addr="));   Serial.println(address);

  return d;
}

#endif  // ESP32

#endif