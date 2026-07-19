/**
 * @file ConfigManager.h
 * @brief LittleFS config loader (ESP32 / LFX_CONFIG_ENABLED only).
 *
 * Single responsibility: mount LittleFS, parse the JSON config, initialise
 * the DeviceFactory and DCC. No HTTP concerns — routes are registered by
 * WebUI or any other API layer via the public helpers below.
 *
 * Must be called before ApiServer::init().
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

  #include "config/DeviceFactory.h"
  #include "dcc/DccDrivable.h"
  #include <Arduino.h>
  #include <LittleFS.h>

class ConfigManager {
public:
  /**
   * @brief Mount LittleFS, load config, init devices and DCC.
   *
   * @param configPath  LittleFS path to the JSON config file (e.g. "/config.json").
   */
  static void init(const char *configPath);

  /** @brief Read-only access to the device factory (for WebUI and other modules). */
  static const DeviceFactory &factory() { return _factory; }

  /** @brief Read the config file from LittleFS. Returns empty String if absent. */
  static String readConfig();

  /** @brief Write (overwrite) the config file on LittleFS. Returns false on error. */
  static bool writeConfig(const String &json);

  /** @brief Delete the config file from LittleFS. */
  static void deleteConfig();

  /** @brief True if the config file exists on LittleFS. */
  static bool configExists();

  /** @brief LittleFS path passed to init(). */
  static const char *configPath() { return _configPath; }

  /**
   * @brief List all *.json files in LittleFS root (excluding board_types.json).
   *
   * @return Comma-separated JSON array string, e.g.
   *         ["config.json","config_2_ext.json"]
   */
  static String listConfigs();

  /**
   * @brief Copy srcFile to configPath (overwrites active config).
   *
   * @param srcFile  LittleFS path of the source file (e.g. "/config_2_ext.json").
   * @return true on success.
   */
  static bool activateConfig(const char *srcFile);

  /** @brief Read any file from LittleFS by path. Returns empty String if absent. */
  static String readFile(const char *path);

  /**
   * @brief Reload config from LittleFS without a reboot.
   *
   * Safe only when no devices are currently running (factory.count() == 0),
   * which is the case right after a first-boot wizard that saved a config with
   * buses and boards but no devices yet.
   *
   * Sequence: resetIfEmpty → BusRegistry::reset → load → initAll → DCC init.
   *
   * @return true on success, false if devices are running or the load fails.
   */
  static bool reload();

  /**
   * @brief Schedule a hot-reload from the HTTP handler (Core 0).
   *
   * Sets a volatile flag; the actual reload is deferred to Core 1 via
   * handlePendingReload(), which must be called from LayoutFX::loop().
   * Safe to call even when devices are running — fullReset() will tear them
   * down cleanly before the new config is applied.
   */
  static void requestReload();

  /**
   * @brief Execute a pending hot-reload if requestReload() was called.
   *
   * Must be called from Core 1 (Arduino loop), strictly between two
   * CoroutineScheduler::loop() passes.  Performs fullReset(), rebuilds buses
   * and devices from LittleFS, then resets the scheduler.
   */
  static void handlePendingReload();

  static constexpr size_t kFsChunkSize = 512; ///< LittleFS read/write chunk size — shared with DeviceApi helpers.

private:

  static DeviceFactory _factory;
  static const char *_configPath;
  static volatile bool _reloadPending;

  static String _readFile(const char *path);
};

#endif // LFX_CONFIG_ENABLED
