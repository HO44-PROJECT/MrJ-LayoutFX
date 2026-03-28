/**
 * @file ConfigManager.h
 *
 * @brief LittleFS config loader + HTTP /config /status endpoints (ESP32 / CONFIG only).
 *
 * Loads the JSON config from LittleFS, runs DeviceFactory, initialises DCC if
 * dcc_pin is present, and registers the /config and /status routes on ApiServer.
 *
 * Must be called before ApiServer::init().
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#ifdef CONFIG
#ifdef ESP32

#include <Arduino.h>
#include "config/DeviceFactory.h"

class ConfigManager {
public:
    /**
     * @brief Mount LittleFS, load config, init devices and DCC, register HTTP endpoints.
     *
     * @param configPath  LittleFS path to the JSON config file (e.g. "/config.json").
     */
    static void init(const char* configPath);

    /** @brief Read-only access to the device factory (for WebUI and other modules). */
    static const DeviceFactory& factory() { return _factory; }

private:
    static DeviceFactory _factory;
    static const char*   _configPath;

    // HTTP handlers (registered on ApiServer)
    static void _onGetConfig();
    static void _onPostConfig();
    static void _onDeleteConfig();
    static void _onGetStatus();

    // LittleFS helpers
    static String _readConfig();
    static bool   _writeConfig(const String& json);
};

#endif  // ESP32
#endif  // CONFIG
