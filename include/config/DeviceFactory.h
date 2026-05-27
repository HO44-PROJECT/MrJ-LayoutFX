/**
 * @file DeviceFactory.h
 * @brief Creates Device instances from a JSON configuration document (ESP32 only).
 *
 * Reads the JSON config, instantiates the correct Device subclass for every entry
 * in "devices", opens the buses described in "buses", sets the DCC address and label.
 *
 * JSON schema summary
 * -------------------
 * {
 *   "buses": {
 *     "<key>": { "type": "dcc",             "pin": <gpio> },
 *     "<key>": { "type": "spi_master_only", "mosi": <gpio>, "sclk": <gpio>, "latch": <gpio> },
 *     "<key>": { "type": "uart",             "tx": <gpio>, "rx": <gpio>, "baud": <int> },
 *     "<key>": { "type": "i2c",              "sda": <gpio>, "scl": <gpio> }
 *   },
 *   "boards": [
 *     { "id": "<str>", "type": "<str>" },
 *     { "id": "<str>", "type": "<str>", "bus": "<key>", "pin_count": <int> }
 *   ],
 *   "devices": [
 *     { "id": "<str>", "type": "<ClassName>",
 *       "board":         "<board-id>",
 *       "wiring":        <int> | [<int>…],
 *       "label":         "<char>",
 *       "address":       <int>,
 *       "default_state": "on"|"off"
 *     }
 *   ]
 * }
 *
 * Wiring semantics — derived from the bus type of the board:
 * ----------------------------------------------------------
 *   board with no bus (root MCU)      → wiring = GPIO pin number
 *   board on spi_master_only bus       → wiring = output bit (1-based in chain)
 *   board on uart bus (LobotChain)     → wiring = servo ID
 *   board on uart bus (DfPlayerMini)   → no wiring (rx/tx from bus)
 *
 * The "type" field of a board entry is a free string used by the frontend to look
 * up the visual definition in board_types.json.  The firmware never interprets it.
 *
 * @note Requires ArduinoJson (>= 6) in lib_deps.
 *       For SerialServo, also requires the MRJFX_LOBOT_SERVO_ENABLED build flag.
 *       UART bus keys must match the hardware serial name (uart0, uart1, uart2).
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author  MrJ
 * @date    2026-04-22
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_CONFIG_ENABLED

  #include "bus/BusRegistry.h"
  #include "config/DeviceFactoryKeys.h"
  #include "devices/Device.h"
  #include "utils/utils.h"
  #include <Arduino.h>
  #include <ArduinoJson.h>

  #ifdef MRJFX_SPI_CARDS_ENABLED
    #include "spi/Spi595Bus.h"
  #endif

  #ifdef MRJFX_LOBOT_SERVO_ENABLED
    #include "servo/LobotServo.h"
  #endif
  #ifdef MRJFX_I2C_DEVICES_ENABLED
    #include <Adafruit_PWMServoDriver.h>
  #endif

class DeviceFactory {
public:
  // ---------------------------------------------------------------------------
  // Size constants (shared by struct fields and local buffers)
  // ---------------------------------------------------------------------------

  static constexpr uint8_t FACTORY_ID_LEN = 32;    ///< Max chars for id / type / bus-key strings (incl. NUL).
  static constexpr uint8_t FACTORY_LABEL_LEN = 48; ///< Max chars for human-readable label strings (incl. NUL).

  // ---------------------------------------------------------------------------
  // Enums
  // ---------------------------------------------------------------------------

  /** @brief Protocol type of a bus entry — resolved at parse time, drives wiring semantics. */
  enum BusType : uint8_t {
    BUS_NONE = 0,       ///< No bus — root MCU board, wiring = GPIO.
    BUS_SPI_MASTER = 1, ///< spi_master_only — wiring = output bit (1-based in daisy-chain).
    BUS_SPI_FULL = 2,   ///< spi_full_duplex — reserved.
    BUS_UART = 3,       ///< uart — wiring = servo ID (LobotChain) or no wiring (DfPlayerMini).
    BUS_I2C = 4,        ///< i2c — reserved.
    BUS_DCC = 5,        ///< dcc — input only, no boards attached.
  };

  /** @brief Hardware type of one SPI slot — used only for Spi595Bus::init(). */
  enum SpiCardType : uint8_t {
    SPI_CARD_UNKNOWN = 0,
    SPI_CARD_HC595 = 1,
  };

  // ---------------------------------------------------------------------------
  // Structs
  // ---------------------------------------------------------------------------

  /** @brief Configuration for one serial port (populated from buses[type=uart]). */
  struct PortCfg {
    char name[FACTORY_ID_LEN]; ///< Bus key, e.g. "uart2".
    HardwareSerial *serial;    ///< Matching ESP32 global (Serial/Serial1/Serial2).
    int tx;
    int rx;
    int baud;
  };

  /** @brief SPI physical bus config (populated from buses[type=spi_master_only]). */
  struct SpiBusCfg {
    int mosi = -1;
    int sclk = -1;
    int latch = -1;
    bool configured() const { return mosi >= 0 && sclk >= 0 && latch >= 0; }
  };

  /** @brief Minimal config for one SPI slot — used to call Spi595Bus::init(). */
  struct SpiCardCfg {
    SpiCardType type = SPI_CARD_UNKNOWN;
    uint8_t pinCount = 0;
    bool configured() const { return type != SPI_CARD_UNKNOWN && pinCount > 0; }
  };

  /**
   * @brief Configuration for one board entry.
   *
   * The "type" string (typeStr) is stored as-is for logging and API responses.
   * The firmware never interprets it — wiring semantics are derived from busType.
   */
  struct BoardCfg {
    char id[FACTORY_ID_LEN] = {};       ///< Unique board identifier.
    char label[FACTORY_LABEL_LEN] = {}; ///< Optional human-readable label.
    char typeStr[FACTORY_ID_LEN] = {};  ///< Board type string (frontend only, e.g. "HC595").
    char busKey[FACTORY_ID_LEN] = {};   ///< Key of the bus this board is on (empty = root board).
    BusType busType = BUS_NONE;         ///< Resolved bus protocol at parse time.
    uint8_t pinCount = 0;               ///< Number of output pins (SPI boards only).
    uint8_t spiRank = 0;                ///< 1-based daisy-chain rank (SPI boards only).
    uint8_t  i2cAddress   = 0x40;       ///< I2C address (I2C boards only, default 0x40).
    uint32_t oscillatorHz = 25000000;   ///< PCA9685 oscillator frequency in Hz (default 25 MHz).

    bool isRoot() const { return busType == BUS_NONE; }
  };

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  /**
   * @brief Parse a JSON config document and instantiate all buses, boards and devices.
   * @param json           Null-terminated JSON string (device config).
   * @param boardTypesJson Optional null-terminated JSON string (board_types.json).
   *                       Used to infer SPI board pin counts when "pin_count" is absent.
   * @return true on success, false if the main JSON fails to parse.
   */
  bool load(const char *json, const char *boardTypesJson = nullptr);

  /** @brief Call initPins() on every Device created by load(). */
  void initAll();

  /**
   * @brief Unconditional full reset: suspend, detach and delete every running
   *        Device (and its associated LobotServo / PCA9685 driver), then clear
   *        all bus/board/port state.
   *
   * MUST be called from the same core as CoroutineScheduler::loop() (Core 1 /
   * Arduino loop), strictly between two scheduler passes.
   * After this call, load() can be called again with a fresh config.
   */
  void fullReset();

  /**
   * @brief Clear all bus/board state so load() can be called again.
   *        Safe only when count() == 0 (no Device objects exist).
   *        Called by ConfigManager::reload() on first-boot wizard apply.
   * @return true if reset succeeded, false if devices are already running.
   */
  bool resetIfEmpty();

  /** @brief Number of devices created by load(). */
  size_t count() const { return _count; }

  /**
   * @brief Access a device by index.
   * @param i Zero-based device index.
   * @return Pointer to the Device, or nullptr if out of range.
   */
  Device *device(size_t i) const { return (i < _count) ? _devices[i] : nullptr; }

  /**
   * @brief Get the config id string for a device.
   * @param i Zero-based device index.
   * @return Null-terminated id string, or "" if out of range.
   */
  const char *deviceId(size_t i) const { return (i < _count) ? _ids[i] : ""; }

  /**
   * @brief Get the 1-based board index for a device (0 = root MCU).
   * @param i Zero-based device index.
   */
  uint8_t deviceBoard(size_t i) const { return (i < _count) ? _boards[i] : 0; }

  const SpiBusCfg &spiBus() const { return _spiBus; }
  uint8_t boardCount() const { return _boardCount; }
  uint8_t spiCardCount() const { return _spiCardCount; }
  int dccPin() const { return _dccPin; }

  /**
   * @brief Access a board configuration by 1-based index.
   * @param i 1-based board index.
   * @return Reference to the BoardCfg, or an empty default if out of range.
   */
  const BoardCfg &board(uint8_t i) const {
    static const BoardCfg empty;
    return (i >= 1 && i <= _boardCount) ? _boards_cfg[i - 1] : empty;
  }

  /**
   * @brief Access an SPI card configuration by 1-based index.
   * @param i 1-based SPI card index.
   * @return Reference to the SpiCardCfg, or an empty default if out of range.
   */
  const SpiCardCfg &spiCard(uint8_t i) const {
    static const SpiCardCfg empty;
    return (i >= 1 && i <= _spiCardCount) ? _spiCards[i - 1] : empty;
  }

private:
  Device *_devices[MRJFX_FACTORY_MAX_DEVICES];
  char _ids[MRJFX_FACTORY_MAX_DEVICES][FACTORY_ID_LEN];
  uint8_t _boards[MRJFX_FACTORY_MAX_DEVICES];
  size_t _count = 0;

  int _dccPin = -1;
  SpiBusCfg _spiBus;

  BoardCfg _boards_cfg[MRJFX_FACTORY_MAX_BOARDS];
  uint8_t _boardCount = 0;

  SpiCardCfg _spiCards[MRJFX_FACTORY_MAX_SPI_CARDS];
  uint8_t _spiCardCount = 0;

  PortCfg _ports[MRJFX_FACTORY_MAX_PORTS];
  size_t _portCount = 0;

  /** @brief Bus key → BusType catalog, populated during _parseBuses(). */
  struct BusEntry {
    char key[FACTORY_ID_LEN] = {};
    BusType type = BUS_NONE;
  };
  BusEntry _busEntries[MRJFX_FACTORY_MAX_BUSES];
  uint8_t _busCount = 0;

  #ifdef MRJFX_LOBOT_SERVO_ENABLED
  ace_routine::Coroutine *_lobotServos[MRJFX_FACTORY_MAX_DEVICES];
  size_t _lobotCount = 0;
  #endif

  #ifdef MRJFX_I2C_DEVICES_ENABLED
  Adafruit_PWMServoDriver *_pwmDrivers[MRJFX_FACTORY_MAX_BOARDS] = {};
  #endif

  bool _parseBuses(JsonObject buses);
  BusType _resolveBusType(const char *busKey) const;
  uint8_t _resolveBoardId(const char *id) const;
  uint8_t _resolveBoardIdx(JsonVariant v) const;

  PortCfg *_findPort(const char *busKey);
  HardwareSerial *_findSerial(const char *busKey);

  PIN_ID _pin(JsonVariant v, uint8_t boardIdx = 0);
  size_t _pins(JsonVariant v, PIN_ID *out, size_t maxPins, uint8_t boardIdx = 0);

  Device *_createDevice(JsonObject obj);
};

#endif // MRJFX_CONFIG_ENABLED
