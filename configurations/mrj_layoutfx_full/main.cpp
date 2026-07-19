/**
 * @file main.cpp
 *
 * @brief mrj_layoutfx_full — factory profile, every feature enabled:
 * WiFi/WebUI/OTA, DCC, SPI, I2C, UART servo, OLED. See config.h for the
 * exact flag set.
 *
 * @workflow
 *   1. pio run -e mrj_layoutfx_full -t upload      (flash firmware)
 *   2. pio run -e mrj_layoutfx_full -t uploadfs    (flash LittleFS — config.json + board_types.json)
 *   3. Open browser   http://<IP>/ui
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#include <LayoutFX.h>

void setup() { LayoutFX::init(); }
void loop() { LayoutFX::loop(); }
