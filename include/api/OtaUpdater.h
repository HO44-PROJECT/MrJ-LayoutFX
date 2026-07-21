/**
 * @file OtaUpdater.h
 * @brief Wireless firmware update for ESP32 — two complementary mechanisms:
 *
 *   1. ArduinoOTA (espota)  — `pio run -t upload` over WiFi, from a dev machine.
 *   2. Web endpoint /update — upload a firmware .bin from any browser (field use).
 *
 * Both are compiled in by `#define OTA` in config.h (→ LFX_OTA_ENABLED).
 * The web endpoint reuses the existing ApiServer WebServer, so it needs
 * LFX_API_SERVER_ENABLED; ArduinoOTA only needs an active WiFi connection.
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_OTA_ENABLED

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
 * @brief Is a firmware transfer (espota or web /update) currently in progress?
 *
 * Both OTA paths block the main loop unevenly while writing flash
 * (Update.write() per chunk), which desyncs the effect coroutines' timing
 * assumptions and makes LEDs flicker instead of pausing cleanly (#133).
 * LayoutFX::loop() checks this to skip effect scheduling/output for the
 * transfer's duration; no restore step is needed since both paths reboot
 * the device on completion.
 */
bool inProgress();

/**
 * @brief mDNS / OTA hostname actually in use, e.g. "layoutfx-1a2b" (OTA_HOSTNAME +
 *        a per-device MAC suffix to avoid collisions). Valid after
 *        beginArduinoOta(); empty string before.
 */
const char *hostname();

  #ifdef LFX_API_SERVER_ENABLED
/**
 * @brief Register GET/POST /update on the shared ApiServer WebServer.
 *        Must be called before ApiServer::init() (i.e. before the server starts).
 */
void registerWebRoutes();
  #endif

} // namespace OtaUpdater

#endif // LFX_OTA_ENABLED
