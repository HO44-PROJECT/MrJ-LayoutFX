/**
 * @file config.h
 *
 * @brief "mini" profile — WiFi/WebUI/OTA + I2C servos + OLED. No DCC, no
 * SPI (74HC595) shift-register bus.
 *
 * Injected into every translation unit via -include in platformio.ini.
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */
#pragma once

// --- Feature flags ------------------------------------------------------
#define CONFIG "config_esp32mini.json" ///< Config JSON file on LittleFS
#define WEBUI ///< Web control panel (GET /ui, /api/devices, POST
// /api/device).
#define I2C_SCAN  ///< Enable GET /api/scan/i2c — I2C bus scanner in the web UI.
#define I2C_CARDS ///< Enable I2C device drivers (PCA9685 servo, etc.).
#define API
#define OTA ///< Enable OTA firmware update (espota + web /update). Optional:
            ///< #define OTA_PASSWORD "…"

// --- WiFi (supprimer les 3 lignes pour désactiver le WiFi) --------------
#include "../auth/wifi.h" // gitignored — définit WIFI_SSID et WIFI_PASSWORD
#define HTTP_PORT 80
#define OLED
#define OLED_SPLASH

#define LOG_SERIAL