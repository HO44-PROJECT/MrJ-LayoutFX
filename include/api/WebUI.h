/**
 * @file WebUI.h
 *
 * @brief Web interface for live device monitoring and on/off control (ESP32 / WEBUI only).
 *
 * Registers routes on ApiServer:
 *   GET  /ui                → serve the control panel HTML page
 *   GET  /api/devices       → JSON array of all devices with current state
 *   POST /api/device        → body {"id":"<id>","state":<0|1>} — toggle a device
 *   POST /api/all           → body {"state":<0|1>} — control all non-static devices
 *   POST /api/group         → body {"type":"<DeviceName>","state":<0|1>} — control a type
 *   GET  /api/config        → download /config.json from LittleFS
 *   POST /api/config        → body <raw JSON> — overwrite /config.json then reboot
 *   GET  /api/status        → JSON object with firmware version and ESP32 metrics
 *   GET  /api/boards        → JSON array of configured boards (id, type, bus, pinCount, spiRank)
 *   GET  /api/board-types   → stream /board_types.json from LittleFS (visual definitions)
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
    static void _onAllDevices();
    static void _onGroupDevices();
    static void _onGetConfig();
    static void _onPostConfig();
    static void _onGetStatus();
    static void _onGetBoards();
    static void _onGetBoardTypes();
    static void _onTestGpio();
    static void _onTestSpi();
};

#endif  // ESP32
#endif  // WEBUI
