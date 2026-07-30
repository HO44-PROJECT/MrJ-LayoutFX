/**
 * @file DeviceApi.h
 * @brief Device REST API — all /api/ routes (ESP32 / LFX_API_SERVER_ENABLED only).
 *
 * Registers and handles the JSON endpoints for device control and system
 * introspection. No HTML — use WebUI for the browser interface.
 *
 * Implementation is split across four translation units:
 *   DeviceApi.cpp       — init() + device control + servo endpoints
 *   DeviceConfigApi.cpp — config file management endpoints
 *   DeviceStatusApi.cpp — status, boards, health, restart endpoints
 *   DeviceTestApi.cpp   — GPIO / SPI / I2C diagnostic endpoints
 *
 * Routes:
 *   GET    /api/devices          — JSON array of all devices with current state
 *   POST   /api/device           — body {"id":"<id>","state":<n>} — set device state
 *   POST   /api/switch           — body {"id":"<id>","on":<bool>} — switchOn / switchOff
 *   POST   /api/all              — body {"state":<n>[,"board":<n>]} — all non-static devices
 *   POST   /api/group            — body {"type":"<name>","state":<n>} — all of one type
 *                                   or {"address":<n>,"state":<n>} — all sharing a DCC address (#10)
 *   POST   /api/servo            — body {"id":"<id>","speed":<n>|"action":"reverse"}
 *   GET    /api/config           — download the active config file from LittleFS
 *   GET    /api/config/file      — download a named config file. Query: ?name=<file>
 *   POST   /api/config           — body <raw JSON> — overwrite active config (no reboot)
 *   DELETE /api/config           — delete active config then restart
 *   POST   /api/config/copy      — body {"from":"<f>","to":"<t>"} — copy config file
 *   POST   /api/config/rename    — body {"from":"<f>","to":"<t>"} — rename config file
 *   POST   /api/config/activate  — body {"file":"<name>"} — set active config
 *   GET    /api/configs          — list all .json config files with active flag
 *   POST   /api/configs          — upload named config. Header: X-Config-Name
 *   DELETE /api/configs          — body {"file":"<name>"} — delete named config
 *   GET    /api/status           — firmware version, IP, heap, LittleFS metrics
 *   GET    /api/boards           — configured boards (id, type, bus, pinCount, spiRank)
 *   GET    /api/board-types      — stream board_types.json from LittleFS
 *   GET    /api/device-types     — stream device_types.json from LittleFS
 *   GET    /api/bus-types        — stream bus_types.json from LittleFS
 *   GET    /api/i2c-known        — stream i2c_known.json from LittleFS
 *   GET    /api/health           — hardware health check for each device (servo ACK, etc.)
 *   GET    /api/dcc-status       — DCC packet counters + last-seen per message category,
 *                                  plus a decoded-event log (oldest to newest)
 *   POST   /api/restart          — immediate ESP32 restart
 *   POST   /api/reload           — re-parse config.json without rebooting (wizard only)
 *   POST   /api/test/gpio        — body {"pin":<n>,"state":<0|1>} — raw GPIO toggle
 *   POST   /api/test/spi         — body {"card":<n>,"channel":<n>,"state":<0|1>} — raw SPI
 *   GET    /api/scan/i2c         — scan I2C bus, return found addresses (requires I2C_SCAN)
 *
 * Must be called after ConfigManager::init() and before ApiServer::init().
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author  MrJ
 * @date    2026-04-22
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_API_SERVER_ENABLED

  #include "api/ApiServer.h"
  #include "api/DeviceApiKeys.h"
  #include "config/ConfigManager.h"
  #include "config/DeviceFactory.h"
  #include <ArduinoJson.h>
  #include <LittleFS.h>
  #include <WiFi.h>
  #include <esp_system.h>

  #ifdef LFX_SPI_CARDS_ENABLED
    #include "spi/Spi595Bus.h"
  #endif
  #ifdef LFX_OLED_ENABLED
    #include "oled/OledDisplay.h"
  #endif
  #ifdef LFX_SERIAL_SERVO_ENABLED
    #include "servo/SerialServoMotorMode.h"
  #endif
  #ifdef LFX_I2C_DEVICES_ENABLED
    #include "servo/I2cPwmMotorDevice.h"
    #include "servo/I2cPwmServoDevice.h"
  #endif
  #ifdef LFX_SERIAL_AUDIO_ENABLED
    #include "audio/DfRobotSerialMP3.h"
  #endif
  #ifdef LFX_I2C_SCAN_ENABLED
    #include <Wire.h>
  #endif
  #ifdef LFX_DCC_ENABLED
    #include "dcc/DccDrivable.h"
  #endif

class DeviceApi {
public:
  /**
   * @brief Register all /api/ routes on ApiServer.
   *
   * @param factory  Read-only reference to the populated DeviceFactory.
   */
  static void init(const DeviceFactory &factory);

private:
  static const DeviceFactory *_factory;

  // ── Device control ────────────────────────────────────────────────────────
  /** @brief GET  /api/devices — JSON array of all devices with current state. */
  static void _onGetDevices();
  /** @brief POST /api/device  — Set one device state. Body: {"id":"<id>","state":<n>}. */
  static void _onPostDevice();
  /** @brief POST /api/switch  — SwitchOn/Off one device. Body: {"id":"<id>","on":<bool>}. */
  static void _onSwitch();
  /** @brief POST /api/all     — Set state on all non-static devices. Body: {"state":<n>[,"board":<n>]}. */
  static void _onAllDevices();
  /** @brief POST /api/group   — Set state on all devices of a type, or all sharing a DCC address (#10). Body: {"type":"<name>","state":<n>} or {"address":<n>,"state":<n>}. */
  static void _onGroupDevices();
  /** @brief POST /api/servo   — Set motor speed or reverse. Body: {"id":"<id>","speed":<n>|"action":"reverse"}. */
  static void _onServo();

  static constexpr size_t kFsNameMax = 32;   ///< Max config filename length incl. NUL (LittleFS constraint).
  static constexpr uint8_t kGpioPinMax = 39; ///< Highest valid GPIO pin number on ESP32.
  static constexpr uint8_t kUart0TxPin = 1;  ///< UART0 TX — reserved, must not be toggled by test endpoints.
  static constexpr uint8_t kUart0RxPin = 3;  ///< UART0 RX — reserved, must not be toggled by test endpoints.

  // ── Config file helpers ───────────────────────────────────────────────────
  /** @brief Validate and normalise a config filename in-place (append .json, reject unsafe chars). */
  static bool _sanitizeCfgName(String &name);
  /** @brief Write a byte buffer to an open File in ConfigManager::kFsChunkSize-byte chunks. */
  static bool _writeAllBytes(File &f, const uint8_t *buf, size_t len);
  /** @brief Streaming file copy within LittleFS; overwrites dst if it exists. */
  static bool _copyFile(const char *src, const char *dst);
  /** @brief Rename a LittleFS file by copy-then-delete (LittleFS has no native rename). */
  static bool _safeRename(const char *from, const char *to);

  // ── Config file management ────────────────────────────────────────────────
  /** @brief GET    /api/config          — Download the active config.json. */
  static void _onGetConfig();
  /** @brief POST   /api/config          — Overwrite the active config.json. Body: raw JSON. */
  static void _onPostConfig();
  /** @brief DELETE /api/config          — Delete config.json then restart. */
  static void _onDeleteConfig();
  /** @brief GET    /api/configs         — List all .json config files with active flag. */
  static void _onGetConfigs();
  /** @brief GET    /api/config/file     — Download a named config file. Query: ?name=<file>. */
  static void _onGetNamedConfig();
  /** @brief POST   /api/configs         — Upload a new named config file. Header: X-Config-Name. */
  static void _onPostNamedConfig();
  /** @brief DELETE /api/configs         — Delete a named config file. Body: {"file":"<name>"}. */
  static void _onDeleteNamedConfig();
  /** @brief POST   /api/config/copy     — Copy a config file. Body: {"from":"<f>","to":"<t>"}. */
  static void _onCopyConfig();
  /** @brief POST   /api/config/rename   — Rename a config file. Body: {"from":"<f>","to":"<t>"}. */
  static void _onRenameConfig();
  /** @brief POST   /api/config/activate — Copy a file to config.json. Body: {"file":"<name>"}. */
  static void _onActivateConfig();

  // ── System info ───────────────────────────────────────────────────────────
  /** @brief GET /api/status      — Firmware version, heap, FS metrics, features, lib versions. */
  static void _onGetStatus();
  /** @brief GET /api/boards      — Configured boards with id, type, bus, pinCount, spiRank. */
  static void _onGetBoards();
  /** @brief GET /api/board-types   — Stream board_types.json from LittleFS. */
  static void _onGetBoardTypes();
  /** @brief GET /api/device-types  — Stream device_types.json from LittleFS. */
  static void _onGetDeviceTypes();
  /** @brief GET /api/bus-types     — Stream bus_types.json from LittleFS. */
  static void _onGetBusTypes();
  /** @brief GET /api/i2c-known     — Stream i2c_known.json from LittleFS. */
  static void _onGetI2cKnown();
  /** @brief GET /api/health      — Per-device hardware health check results. */
  static void _onGetHealth();
  /** @brief GET /api/dcc-status  — DCC packet counters, last-seen per category, and decoded-event log. */
  static void _onGetDccStatus();
  /** @brief POST /api/restart    — Immediate ESP32 restart. */
  static void _onRestart();
  /** @brief POST /api/reload    — Re-parse config.json without rebooting. Returns 409 if devices are already loaded. */
  static void _onReload();

  // ── Hardware diagnostics ──────────────────────────────────────────────────
  /** @brief POST /api/test/gpio  — Raw GPIO write. Body: {"pin":<n>,"state":<0|1>}. */
  static void _onTestGpio();
  /** @brief POST /api/test/spi   — Raw SPI channel write. Body: {"card":<n>,"channel":<n>,"state":<0|1>}. */
  static void _onTestSpi();
  /** @brief POST /api/test/identify — Blink a pin to locate its LED. Body: {"pin":<n>} | {"card":<n>,"channel":<n>} | {} to stop. */
  static void _onIdentify();
  #ifdef LFX_I2C_SCAN_ENABLED
  /** @brief GET /api/scan/i2c   — Scan I2C bus and return found addresses. */
  static void _onScanI2c();
  #endif
};

#endif // LFX_API_SERVER_ENABLED
