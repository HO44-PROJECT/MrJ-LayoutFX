/**
 * @file WebUI.h
 *
 * @brief Web interface for live device monitoring and on/off control (ESP32 / WEBUI only).
 *
 * Registers three routes on ApiServer:
 *   GET  /ui          → serve the control panel HTML page
 *   GET  /api/devices → JSON array of all devices with current state
 *   POST /api/device  → body {"id":"<id>","state":<0|1>} — toggle a device
 *
 * Must be called after ConfigManager::init() and before ApiServer::init().
 *
 * Typical usage
 * -------------
 *   ConfigManager::init(CONFIG_PATH);
 *   WebUI::init(ConfigManager::factory());
 *   ApiServer::init(WIFI_SSID, WIFI_PASSWORD);
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#ifdef WEBUI
#ifdef ESP32

#include "config/DeviceFactory.h"

class WebUI {
public:
    /**
     * @brief Register /ui, /api/devices, /api/device routes on ApiServer.
     *
     * @param factory  Read-only reference to the populated DeviceFactory.
     *                 (Device pointers inside are non-const so newState() works.)
     */
    static void init(const DeviceFactory& factory);

private:
    static const DeviceFactory* _factory;

    static void _onGetUi();
    static void _onGetDevices();
    static void _onPostDevice();
};

#endif  // ESP32
#endif  // WEBUI
