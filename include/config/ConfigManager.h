/**
 * @file ConfigManager.h
 *
 * @brief LittleFS config loader (ESP32 / MRJFX_CONFIG_ENABLED only).
 *
 * Single responsibility: mount LittleFS, parse the JSON config, initialise
 * the DeviceFactory and DCC. No HTTP concerns — routes are registered by
 * WebUI or any other API layer via the public helpers below.
 *
 * Must be called before ApiServer::init().
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_CONFIG_ENABLED

#include <Arduino.h>
#include "config/DeviceFactory.h"

class ConfigManager {
public:
    /**
     * @brief Mount LittleFS, load config, init devices and DCC.
     *
     * @param configPath  LittleFS path to the JSON config file (e.g. "/config.json").
     */
    static void init(const char* configPath);

    /** @brief Read-only access to the device factory (for WebUI and other modules). */
    static const DeviceFactory& factory() { return _factory; }

    /** @brief Read the config file from LittleFS. Returns empty String if absent. */
    static String readConfig();

    /** @brief Write (overwrite) the config file on LittleFS. Returns false on error. */
    static bool writeConfig(const String& json);

    /** @brief Delete the config file from LittleFS. */
    static void deleteConfig();

    /** @brief True if the config file exists on LittleFS. */
    static bool configExists();

    /** @brief LittleFS path passed to init(). */
    static const char* configPath() { return _configPath; }

private:
    static DeviceFactory _factory;
    static const char*   _configPath;

    static String _readFile(const char* path);
};

#endif  // MRJFX_CONFIG_ENABLED
