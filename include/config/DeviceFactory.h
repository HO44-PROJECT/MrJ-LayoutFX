/**
 * @file DeviceFactory.h
 *
 * @brief Creates Device instances from a JSON configuration document (ESP32 only).
 *
 * @objective Read the JSON schema agreed for MrJ-ArduinoRailwayFX, instantiate the
 *            correct Device subclass for every entry in "devices", open the UART ports
 *            described in "serial_ports", set the DCC address, and set the label.
 *
 *            JSON schema summary
 *            -------------------
 *            {
 *              "system":       { "dcc_pin": <gpio> },
 *              "serial_ports": {
 *                "<name>": { "tx": <gpio>, "rx": <gpio>, "baud": <int> }
 *              },
 *              "devices": [
 *                { "id": "<str>", "type": "<ClassName>",
 *                  "label":   "<char>",           // optional, 1 character
 *                  "wiring":  <gpio> | [<gpio>…], // GPIO pin(s) OR servo-bus ID
 *                  "port":    "<name>",            // uart key for serial devices
 *                  "address":       <int>,         // DCC address (0 = not registered)
 *                  "default_state": "on"|"off"    // initial state at boot (default: off)
 *                }
 *              ]
 *            }
 *
 *            Supported types
 *            ---------------
 *            Static      : StaticLow (1-4 pins OUTPUT LOW — suppresses boot pull-ups)
 *            Single-pin  : Beacon, CampFire, DefectLamp, ElectricLamp, GasLamp, NeonSign,
 *                          OilLamp, RailwayCrossingLights, SignalFlare, SolderLamp, Storm,
 *                          Torch, TrainHeadLamp, TurnSignal
 *            Two-pin     : DoubleBeacon
 *            3-pin signal: MrJDBEntrySignal, MrJDBExitSignal, TrafficLight3Phase
 *            2-pin signal: MrJDBBlocSignal
 *            4-pin signal: TrafficLight4Phase
 *            Serial servo: SerialServo  (requires LOBOT build flag)
 *            Serial audio: DfAudio
 *
 * @note Requires ArduinoJson (>= 6) in lib_deps.
 *       For SerialServo, also requires the LOBOT build flag.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#ifdef ESP32

#include <Arduino.h>
#include <ArduinoJson.h>
#include "devices/Device.h"

static constexpr uint8_t FACTORY_MAX_DEVICES = 16;
static constexpr uint8_t FACTORY_MAX_PORTS   =  4;

class DeviceFactory {
public:

  /**
   * @brief Configuration for one serial port entry from "serial_ports".
   */
  struct PortCfg {
    char             name[8];   ///< Key from JSON, e.g. "uart2".
    HardwareSerial*  serial;    ///< Pointer to the matching ESP32 global (Serial1/2).
    int              tx;        ///< TX GPIO, or -1 if unspecified.
    int              rx;        ///< RX GPIO, or -1 if unspecified.
    int              baud;      ///< Baud rate.
  };

  /**
   * @brief Parse a JSON config string, open serial ports, and create all devices.
   *
   * @param json  NUL-terminated UTF-8 JSON string (e.g., read from LittleFS).
   * @return true on success; false if JSON cannot be parsed.
   */
  bool load(const char* json);

  /** @brief Number of successfully created devices. */
  size_t  count()           const { return _count; }

  /** @brief Device at index i, or nullptr if i >= count(). */
  Device* device(size_t i)  const { return (i < _count) ? _devices[i] : nullptr; }

  /** @brief DCC input pin from system.dcc_pin, or -1 if not configured. */
  int     dccPin()          const { return _dccPin; }

  /**
   * @brief Call initPins() on every created device.
   *
   * Invoke once in setup(), after DccDrivable::init().
   * Devices that configure their pins internally (e.g., DfAudio) can safely
   * ignore the call because their initPins() is a no-op or overridden.
   */
  void initAll();

private:
  Device*  _devices[FACTORY_MAX_DEVICES];
  size_t   _count     = 0;
  int      _dccPin    = -1;   ///< From system.dcc_pin, -1 if absent.

  PortCfg  _ports[FACTORY_MAX_PORTS];
  size_t   _portCount = 0;

#ifdef LOBOT
  // LobotServo objects owned by the factory (SerialServoMotor holds a pointer, not ownership)
  ace_routine::Coroutine* _lobotServos[FACTORY_MAX_DEVICES];
  size_t                  _lobotCount = 0;
#endif

  bool    _parsePorts(JsonObject ports);
  Device* _createDevice(JsonObject obj);

  PortCfg* _findPort(const char* portName);
  HardwareSerial* _findSerial(const char* portName);

  /** Extract a single PIN_ID from a JsonVariant (scalar or first element of array). */
  static PIN_ID  _pin (JsonVariant v);

  /** Fill pins[] from a JsonVariant (scalar → pins[0]; array → pins[0..N-1]). */
  static size_t  _pins(JsonVariant v, PIN_ID* out, size_t maxPins);
};

#endif  // ESP32
