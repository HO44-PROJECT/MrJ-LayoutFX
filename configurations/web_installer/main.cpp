/**
 * @file main.cpp
 *
 * @brief poc_ui — JSON config + web control panel at http://<IP>/ui
 *
 * @workflow
 *   1. pio run -e poc_ui -t upload          (flash firmware)
 *   2. pio run -e poc_ui -t uploadfs         (flash LittleFS — config.json +
 * board_types.json)
 *   3. POST /config   curl -X POST http://<IP>/config -d @data/config.json
 *   4. Open browser   http://<IP>/ui
 *
 * @project MrJ-LayoutFX-dev
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include <LayoutFX.h>

void setup() { LayoutFX::init(); }
void loop() { LayoutFX::loop(); }
