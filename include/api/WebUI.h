/**
 * @file WebUI.h
 *
 * @brief Browser interface — serves the control panel HTML page (ESP32 / LFX_WEBUI_ENABLED only).
 *
 * Registers a single route:
 *   GET /ui   → gzipped HTML page (built from src/web/webui.html by tools/build_webui.py)
 *
 * The page communicates with the device through the /api/ routes provided by
 * DeviceApi. WebUI has no knowledge of devices or config — it only serves HTML.
 *
 * Must be called after DeviceApi::init() and before ApiServer::init().
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_WEBUI_ENABLED

class WebUI {
public:
    /** @brief Register GET /ui on ApiServer. */
    static void init();

private:
    static void _onGetUi();
};

#endif  // LFX_WEBUI_ENABLED
