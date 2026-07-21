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
 *       For SerialServo, also requires the LFX_LOBOT_SERVO_ENABLED build flag.
 *       UART bus keys must match the hardware serial name (uart0, uart1, uart2).
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author  MrJ
 * @date    2026-04-22
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_CONFIG_ENABLED

  #include "bus/BusRegistry.h"
  #include "config/BoardPinCount.h"
  #include "config/DeviceFactoryKeys.h"
  #include "devices/Device.h"
  #include "utils/utils.h"
  #include <Arduino.h>
  #include <ArduinoJson.h>

  #ifdef LFX_SPI_CARDS_ENABLED
    #include "spi/Spi595Bus.h"
  #endif

  #ifdef LFX_LOBOT_SERVO_ENABLED
    #include "servo/LobotServo.h"
  #endif
  #ifdef LFX_I2C_DEVICES_ENABLED
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
    uint8_t  oledHeight   = 64;         ///< SSD1306 panel height in px (SSD1306 boards only, default 64).

    bool isRoot() const { return busType == BUS_NONE; }
  };

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  /**
   * @brief Parse a JSON config document and instantiate all buses, boards and devices.
   * @param json         Null-terminated JSON string (device config).
   * @param btPinCounts  Optional structural pin-count map (board type → pin count),
   *                     generated from board_types.json into
   *                     embedded_board_pincounts.h.  Used to size SPI daisy-chain
   *                     cards when a board entry omits "pin_count".
   * @param btCount      Number of entries in @p btPinCounts.
   * @return true on success, false if the main JSON fails to parse.
   */
  bool load(const char *json, const BtPinCount *btPinCounts = nullptr, uint8_t btCount = 0);

  /** @brief Call initPins() on every Device created by load(). */
  void initAll();

  /**
   * @brief Apply default states to all devices (from config["devices"][i]["default_state"]).
   *        Call after initAll() to override the hardcoded OFF_STATE from initPins().
   */
  void applyDefaultStates();

  /**
   * @brief Drive every GPIO listed in config["idle_pins"] to OUTPUT LOW.
   *
   * Called after initAll() to silence unassigned output pins that would
   * otherwise float and cause LED flicker via crosstalk.  Safe to call when
   * idle_pins is empty (no-op).
   */
  void initIdlePins();

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
   * @brief Reverse-lookup the config id string for a device pointer.
   *        Used by the DCC dispatch path (DccDrivable only holds a Device*,
   *        never a factory index) to report OLED events (#46).
   * @param dev Device pointer, as held by DccDrivable::DccDrivableDevices[].
   * @return Null-terminated id string, or "" if not found.
   */
  const char *idOf(const Device *dev) const {
    for (size_t i = 0; i < _count; i++)
      if (_devices[i] == dev)
        return _ids[i];
    return "";
  }

  /**
   * @brief Get the 1-based board index for a device (0 = root MCU).
   * @param i Zero-based device index.
   */
  uint8_t deviceBoard(size_t i) const { return (i < _count) ? _boards[i] : 0; }

  const SpiBusCfg &spiBus() const { return _spiBus; }
  uint8_t boardCount() const { return _boardCount; }
  uint8_t spiCardCount() const { return _spiCardCount; }
  // DCC pin from the config "dcc" bus (-1 when absent). DCC is activated by ADDING
  // the dcc bus in the WebUI (like the uart0 log bus), not by the compile flag alone.
  int dccPin() const { return _dccPin; }
  const char *configName() const { return _configName; }

  /// @brief How the loaded config addresses the UART0 serial-log bus.
  ///   ON      = a "uart0" bus is declared  → keep serial logging, reserve GPIO1/3.
  ///   OFF     = a "buses" section exists without uart0 → release the log + GPIO1/3.
  ///   DEFAULT = no "buses" section at all   → keep the compiled LOG_SERIAL default.
  enum LogBusReq { LOG_BUS_DEFAULT, LOG_BUS_ON, LOG_BUS_OFF };
  LogBusReq logBusRequest() const {
    return _uart0LogBus ? LOG_BUS_ON : (_busesSection ? LOG_BUS_OFF : LOG_BUS_DEFAULT);
  }

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
   * @brief Look up the SSD1306 board declared in the loaded config, if any (#51).
   *        Used by ConfigManager to drive OledDisplay::configure() so the
   *        structural OLED's presence and resolution follow the config,
   *        instead of being fixed at compile time.
   * @param heightOut Set to the board's oled_height when found (untouched otherwise).
   * @return true if an SSD1306 board is declared, false otherwise.
   */
  bool findOledBoard(uint8_t &heightOut) const {
    for (uint8_t i = 0; i < _boardCount; i++) {
      if (strcmp(_boards_cfg[i].typeStr, factory_keys::kBoardTypeSSD1306) == 0) {
        heightOut = _boards_cfg[i].oledHeight;
        return true;
      }
    }
    return false;
  }

private:
  Device *_devices[LFX_FACTORY_MAX_DEVICES];
  char _ids[LFX_FACTORY_MAX_DEVICES][FACTORY_ID_LEN];
  uint8_t _boards[LFX_FACTORY_MAX_DEVICES];
  STATE_TYPE _deviceDefaultStates[LFX_FACTORY_MAX_DEVICES]; ///< Default states from JSON config.
  size_t _count = 0;

  char _configName[FACTORY_LABEL_LEN] = {}; ///< Config "name" field, for OLED display.

  int _dccPin = -1;
  SpiBusCfg _spiBus;

  BoardCfg _boards_cfg[LFX_FACTORY_MAX_BOARDS];
  uint8_t _boardCount = 0;

  // SPI cards seen during parse — kept only to bound spiRank and enforce the
  // FACTORY_MAX_SPI_CARDS cap. The actual pin-count table used by Spi595Bus::init
  // lives in BusRegistry (fed via regSpiCard), so no per-card array is stored here.
  uint8_t _spiCardCount = 0;

  PortCfg _ports[LFX_FACTORY_MAX_PORTS];
  size_t _portCount = 0;

  /** @brief Bus key → BusType catalog, populated during _parseBuses(). */
  struct BusEntry {
    char key[FACTORY_ID_LEN] = {};
    BusType type = BUS_NONE;
  };
  BusEntry _busEntries[LFX_FACTORY_MAX_BUSES];
  uint8_t _busCount = 0;
  bool _busesSection = false; ///< A "buses" object was present in the loaded config.
  bool _uart0LogBus = false;  ///< Config declares the uart0 serial-log bus (keep logging + reserve 1/3).

  #ifdef LFX_LOBOT_SERVO_ENABLED
  ace_routine::Coroutine *_lobotServos[LFX_FACTORY_MAX_DEVICES];
  size_t _lobotCount = 0;
  #endif

  #ifdef LFX_I2C_DEVICES_ENABLED
  Adafruit_PWMServoDriver *_pwmDrivers[LFX_FACTORY_MAX_BOARDS] = {};
  #endif

  static constexpr uint8_t MAX_IDLE_PINS = 32; ///< Max entries in idle_pins[].
  uint8_t _idlePins[MAX_IDLE_PINS] = {};        ///< GPIO pin numbers to drive OUTPUT LOW.
  uint8_t _idlePinCount = 0;

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

#endif // LFX_CONFIG_ENABLED
