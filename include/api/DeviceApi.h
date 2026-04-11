/**
 * @file DeviceApi.h
 *
 * @brief Device REST API — all /api/ routes (ESP32 / MRJFX_API_SERVER_ENABLED only).
 *
 * Registers and handles the JSON endpoints for device control and system
 * introspection. No HTML — use WebUI for the browser interface.
 *
 * Routes:
 *   GET  /api/devices        — JSON array of all devices with current state
 *   POST /api/device         — body {"id":"<id>","state":<n>} — set device state
 *   POST /api/switch         — body {"id":"<id>","on":<bool>} — switchOn / switchOff
 *   POST /api/all            — body {"state":<n>[,"board":<n>]} — all non-static devices
 *   POST /api/group          — body {"type":"<name>","state":<n>} — all of one type
 *   GET  /api/config         — download /config.json from LittleFS
 *   POST /api/config         — body <raw JSON> — overwrite /config.json (no reboot)
 *   DELETE /api/config       — delete /config.json then reboot
 *   POST /api/restart        — body {} — immediate ESP32 restart
 *   POST /api/servo          — body {"id":"<id>","speed":<-1000..1000>} — set motor speed
 *   GET  /api/status         — firmware version, IP, heap, LittleFS metrics
 *   GET  /api/boards         — configured boards (id, type, bus, pinCount, spiRank)
 *   GET  /api/board-types    — stream /board_types.json from LittleFS
 *   GET  /api/health         — hardware health check for each device (servo ACK, etc.)
 *   POST /api/test/gpio      — body {"pin":<n>,"state":<0|1>} — raw GPIO toggle
 *   POST /api/test/spi       — body {"card":<n>,"channel":<n>,"state":<0|1>} — raw SPI
 *
 * Must be called after ConfigManager::init() and before ApiServer::init().
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_API_SERVER_ENABLED

#include "config/DeviceFactory.h"

class DeviceApi {
public:
    /**
     * @brief Register all /api/ routes on ApiServer.
     *
     * @param factory  Read-only reference to the populated DeviceFactory.
     */
    static void init(const DeviceFactory& factory);

private:
    static const DeviceFactory* _factory;

    static void _onGetDevices();
    static void _onPostDevice();
    static void _onSwitch();
    static void _onAllDevices();
    static void _onGroupDevices();
    static void _onGetConfig();
    static void _onPostConfig();
    static void _onDeleteConfig();
    static void _onGetConfigs();
    static void _onActivateConfig();
    static void _onGetStatus();
    static void _onGetBoards();
    static void _onGetBoardTypes();
    static void _onGetHealth();
    static void _onTestGpio();
    static void _onTestSpi();
    static void _onRestart();
    static void _onServo();
};

#endif  // MRJFX_API_SERVER_ENABLED
