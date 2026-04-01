/**
 * @file DeviceFactory.h
 *
 * @brief Creates Device instances from a JSON configuration document (ESP32 only).
 *
 * @objective Read the JSON config, instantiate the correct Device subclass for every
 *            entry in "devices", open the buses described in "buses", set the DCC
 *            address, and set the label.
 *
 *            JSON schema summary
 *            -------------------
 *            {
 *              "buses": {
 *                "<key>": { "type": "dcc",             "pin": <gpio> },
 *                "<key>": { "type": "spi_master_only", "mosi": <gpio>, "sclk": <gpio>, "latch": <gpio> },
 *                "<key>": { "type": "uart",             "tx": <gpio>, "rx": <gpio>, "baud": <int> },
 *                "<key>": { "type": "i2c",              "sda": <gpio>, "scl": <gpio> }
 *              },
 *              "boards": [
 *                { "id": "<str>", "type": "<str>" },
 *                { "id": "<str>", "type": "<str>", "bus": "<key>", "pin_count": <int> }
 *              ],
 *              "devices": [
 *                { "id": "<str>", "type": "<ClassName>",
 *                  "board":         "<board-id>",
 *                  "wiring":        <int> | [<int>…],
 *                  "label":         "<char>",
 *                  "address":       <int>,
 *                  "default_state": "on"|"off"
 *                }
 *              ]
 *            }
 *
 *            Wiring semantics — derived from the bus type of the board:
 *            -----------------------------------------------------------
 *            board with no bus (root MCU)      → wiring = GPIO pin number
 *            board on spi_master_only bus       → wiring = output bit (1-based in chain)
 *            board on uart bus (LobotChain)     → wiring = servo ID
 *            board on uart bus (DfPlayerMini)   → no wiring (rx/tx from bus)
 *
 *            The "type" field of a board entry is a free string used by the
 *            frontend to look up the visual definition in board_types.json.
 *            The firmware never interprets it.
 *
 * @note Requires ArduinoJson (>= 6) in lib_deps.
 *       For SerialServo, also requires the LOBOT build flag.
 *       UART bus keys must match the hardware serial name (uart0, uart1, uart2).
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#ifdef ESP32

#include <Arduino.h>
#include <ArduinoJson.h>
#include "devices/Device.h"

static constexpr uint8_t FACTORY_MAX_DEVICES    = 24;
static constexpr uint8_t FACTORY_MAX_PORTS      =  4;
static constexpr uint8_t FACTORY_MAX_BOARDS     = 12;
static constexpr uint8_t FACTORY_MAX_BUSES      =  8;
static constexpr uint8_t FACTORY_MAX_BOARD_TYPES = 16;  ///< Max entries from board_types.json.

class DeviceFactory {
public:

  // ---------------------------------------------------------------------------
  // Enums
  // ---------------------------------------------------------------------------

  /** @brief Protocol type of a bus entry — resolved at parse time, drives wiring semantics. */
  enum BusType : uint8_t {
    BUS_NONE       = 0,  ///< No bus — root MCU board, wiring = GPIO.
    BUS_SPI_MASTER = 1,  ///< spi_master_only — wiring = output bit (1-based in daisy-chain).
    BUS_SPI_FULL   = 2,  ///< spi_full_duplex — reserved.
    BUS_UART       = 3,  ///< uart — wiring = servo ID (LobotChain) or no wiring (DfPlayerMini).
    BUS_I2C        = 4,  ///< i2c — reserved.
    BUS_DCC        = 5,  ///< dcc — input only, no boards attached.
  };

  /** @brief Hardware type of one SPI slot — used only for Spi595Bus::init(). */
  enum SpiCardType : uint8_t {
    SPI_CARD_UNKNOWN = 0,
    SPI_CARD_HC595   = 1,
  };

  // ---------------------------------------------------------------------------
  // Structs
  // ---------------------------------------------------------------------------

  /** @brief Configuration for one serial port (populated from buses[type=uart]). */
  struct PortCfg {
    char            name[32];  ///< Bus key, e.g. "uart2".
    HardwareSerial* serial;    ///< Matching ESP32 global (Serial/Serial1/Serial2).
    int             tx;
    int             rx;
    int             baud;
  };

  /** @brief SPI physical bus config (populated from buses[type=spi_master_only]). */
  struct SpiBusCfg {
    int mosi  = -1;
    int sclk  = -1;
    int latch = -1;
    bool configured() const { return mosi >= 0 && sclk >= 0 && latch >= 0; }
  };

  /** @brief Minimal config for one SPI slot — used to call Spi595Bus::init(). */
  struct SpiCardCfg {
    SpiCardType type     = SPI_CARD_UNKNOWN;
    uint8_t     pinCount = 0;
    bool configured() const { return type != SPI_CARD_UNKNOWN && pinCount > 0; }
  };

  /**
   * @brief Configuration for one board entry.
   *
   * The "type" string (typeStr) is stored as-is for logging and API responses.
   * The firmware never interprets it — wiring semantics are derived from busType.
   */
  struct BoardCfg {
    char    id[32]      = {};  ///< Unique board identifier.
    char    label[48]   = {};  ///< Optional human-readable label.
    char    typeStr[32] = {};  ///< Board type string (frontend only, e.g. "HC595").
    char    busKey[32]  = {};  ///< Key of the bus this board is on (empty = root board).
    BusType busType     = BUS_NONE;  ///< Resolved bus protocol at parse time.
    uint8_t pinCount    = 0;   ///< Number of output pins (SPI boards only).
    uint8_t spiRank     = 0;   ///< 1-based daisy-chain rank (SPI boards only).

    bool isRoot() const { return busType == BUS_NONE; }
  };

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  bool load(const char* json, const char* boardTypesJson = nullptr);
  void initAll();

  size_t      count()               const { return _count; }
  Device*     device(size_t i)      const { return (i < _count) ? _devices[i] : nullptr; }
  const char* deviceId(size_t i)    const { return (i < _count) ? _ids[i] : ""; }
  uint8_t     deviceBoard(size_t i) const { return (i < _count) ? _boards[i] : 0; }

  const SpiBusCfg& spiBus()        const { return _spiBus; }
  uint8_t          boardCount()    const { return _boardCount; }
  uint8_t          spiCardCount()  const { return _spiCardCount; }
  int              dccPin()        const { return _dccPin; }

  const BoardCfg& board(uint8_t i) const {
    static const BoardCfg empty;
    return (i >= 1 && i <= _boardCount) ? _boards_cfg[i - 1] : empty;
  }

  const SpiCardCfg& spiCard(uint8_t i) const {
    static const SpiCardCfg empty;
    return (i >= 1 && i <= _spiCardCount) ? _spiCards[i - 1] : empty;
  }

private:
  static constexpr uint8_t FACTORY_MAX_SPI_CARDS = 8;

  Device*    _devices[FACTORY_MAX_DEVICES];
  char       _ids[FACTORY_MAX_DEVICES][32];
  uint8_t    _boards[FACTORY_MAX_DEVICES];
  size_t     _count        = 0;

  int        _dccPin       = -1;
  SpiBusCfg  _spiBus;

  BoardCfg   _boards_cfg[FACTORY_MAX_BOARDS];
  uint8_t    _boardCount   = 0;

  SpiCardCfg _spiCards[FACTORY_MAX_SPI_CARDS];
  uint8_t    _spiCardCount = 0;

  PortCfg  _ports[FACTORY_MAX_PORTS];
  size_t   _portCount = 0;

  /** @brief Bus key → BusType catalog, populated during _parseBuses(). */
  struct BusEntry {
    char    key[32] = {};
    BusType type    = BUS_NONE;
  };
  BusEntry _busEntries[FACTORY_MAX_BUSES];
  uint8_t  _busCount = 0;

#ifdef LOBOT
  ace_routine::Coroutine* _lobotServos[FACTORY_MAX_DEVICES];
  size_t                  _lobotCount = 0;
#endif

  bool    _parseBuses(JsonObject buses);
  BusType _resolveBusType(const char* busKey) const;
  uint8_t _resolveBoardId (const char* id)    const;
  uint8_t _resolveBoardIdx(JsonVariant v)     const;

  PortCfg*        _findPort  (const char* busKey);
  HardwareSerial* _findSerial(const char* busKey);

  PIN_ID _pin (JsonVariant v, uint8_t boardIdx = 0);
  size_t _pins(JsonVariant v, PIN_ID* out, size_t maxPins, uint8_t boardIdx = 0);

  Device* _createDevice(JsonObject obj);
};

#endif  // ESP32
