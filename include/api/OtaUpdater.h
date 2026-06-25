/**
 * @file OtaUpdater.h
 * @brief Wireless firmware update for ESP32 — two complementary mechanisms:
 *
 *   1. ArduinoOTA (espota)  — `pio run -t upload` over WiFi, from a dev machine.
 *   2. Web endpoint /update — upload a firmware .bin from any browser (field use).
 *
 * Both are compiled in by `#define OTA` in config.h (→ MRJFX_OTA_ENABLED).
 * The web endpoint reuses the existing ApiServer WebServer, so it needs
 * MRJFX_API_SERVER_ENABLED; ArduinoOTA only needs an active WiFi connection.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_OTA_ENABLED

namespace OtaUpdater {

/**
 * @brief Configure and start ArduinoOTA (hostname, optional password, callbacks).
 *        Call once, after WiFi is up (STA or AP). Also starts mDNS so the device
 *        is reachable as `<hostname()>.local`.
 */
void beginArduinoOta();

/** @brief Pump ArduinoOTA. Call every iteration of the main loop. */
void handle();

/**
 * @brief mDNS / OTA hostname actually in use, e.g. "mrjfx-1a2b" (OTA_HOSTNAME +
 *        a per-device MAC suffix to avoid collisions). Valid after
 *        beginArduinoOta(); empty string before.
 */
const char *hostname();

  #ifdef MRJFX_API_SERVER_ENABLED
/**
 * @brief Register GET/POST /update on the shared ApiServer WebServer.
 *        Must be called before ApiServer::init() (i.e. before the server starts).
 */
void registerWebRoutes();
  #endif

} // namespace OtaUpdater

#endif // MRJFX_OTA_ENABLED
