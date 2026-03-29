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

static constexpr uint8_t FACTORY_MAX_DEVICES = 24;
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

  /** @brief SPI bus shared by all HC595 daughter cards in daisy-chain. */
  struct SpiBusCfg {
    int mosi  = -1;  ///< MOSI GPIO pin (data into first register).
    int sclk  = -1;  ///< SCLK GPIO pin (shared clock).
    int latch = -1;  ///< Latch GPIO pin (ST_CP, shared — pulses once for the full chain).

    bool configured() const { return mosi >= 0 && sclk >= 0 && latch >= 0; }
  };

  /** @brief Hardware type of one daughter card slot. */
  enum SpiCardType : uint8_t {
    SPI_CARD_UNKNOWN = 0,
    SPI_CARD_HC595   = 1,  ///< 74HC595 shift register — output only (H/L).
  };

  /** @brief Configuration for one HC595 daughter card slot (one entry in spi_cards[]). */
  struct SpiCardCfg {
    SpiCardType type     = SPI_CARD_UNKNOWN;
    uint8_t     pinCount = 0;  ///< Number of output pins on this card (multiple of 8).

    bool configured() const { return type != SPI_CARD_UNKNOWN && pinCount > 0; }
  };

  /** @brief Number of successfully created devices. */
  size_t  count()              const { return _count; }

  /** @brief Device at index i, or nullptr if i >= count(). */
  Device* device(size_t i)     const { return (i < _count) ? _devices[i] : nullptr; }

  /** @brief Id of device at index i (from JSON "id" field), or "" if i >= count(). */
  const char* deviceId(size_t i) const { return (i < _count) ? _ids[i] : ""; }

  /**
   * @brief Daughter SPI card index for device i (1-based), or 0 for main ESP32 GPIO.
   *        Parsed from the optional JSON "board" field.
   */
  uint8_t deviceBoard(size_t i) const { return (i < _count) ? _boards[i] : 0; }

  /** @brief SPI physical bus config from system.spi_bus, or unconfigured if absent. */
  const SpiBusCfg&  spiBus()                    const { return _spiBus; }

  /** @brief Number of configured SPI daughter cards. */
  uint8_t           spiCardCount()              const { return _spiCardCount; }

  /** @brief Config for daughter card at 1-based index i, or unconfigured if out of range. */
  const SpiCardCfg& spiCard(uint8_t i)          const {
      static const SpiCardCfg empty;
      return (i >= 1 && i <= _spiCardCount) ? _spiCards[i - 1] : empty;
  }

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
  static constexpr uint8_t FACTORY_MAX_SPI_CARDS = 8;

  Device*    _devices[FACTORY_MAX_DEVICES];
  char       _ids[FACTORY_MAX_DEVICES][32];
  uint8_t    _boards[FACTORY_MAX_DEVICES];        ///< Daughter card index per device (0 = main ESP32).
  size_t     _count         = 0;
  int        _dccPin        = -1;                 ///< From system.dcc_pin, -1 if absent.
  SpiBusCfg  _spiBus;                             ///< From system.spi_bus.
  SpiCardCfg _spiCards[FACTORY_MAX_SPI_CARDS];    ///< From system.spi_cards[].
  uint8_t    _spiCardCount  = 0;                  ///< Number of entries parsed in spi_cards[].

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

  /**
   * @brief Extract a PIN_ID from a JsonVariant, incorporating the board field.
   *
   * @param v      "wiring" JSON value (scalar or array — only first element used).
   * @param board  "board" JSON value (0 = native GPIO, 1-N = SPI daughter card).
   *               When SPI_CARDS is defined and board > 0, returns PIN_ID::spi(board, bit).
   *               Otherwise returns the raw wiring value as a GPIO pin.
   *               Without SPI_CARDS, board > 0 logs a warning and returns NO_PIN.
   */
  static PIN_ID  _pin (JsonVariant v, uint8_t board = 0);

  /** Fill pins[] from a JsonVariant (scalar → pins[0]; array → pins[0..N-1]).
   *  board is always 0 for multi-pin devices (CharliePlexing — native GPIO only). */
  static size_t  _pins(JsonVariant v, PIN_ID* out, size_t maxPins, uint8_t board = 0);
};

#endif  // ESP32
