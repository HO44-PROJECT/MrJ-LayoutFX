/**
 * @file config.h
 *
 * @brief "full" profile — every feature enabled.
 *
 * Injected into every translation unit via -include in platformio.ini.
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */
#pragma once

// --- Feature flags ------------------------------------------------------
#define CONFIG                                                                 \
  "config.json" ///< Config JSON file on LittleFS — change to "config_taya.json"
#define SPI_CARDS ///< Enable 74HC595 SPI shift-register bus.
#define I2C_CARDS ///< Enable I2C device drivers (PCA9685 servo, etc.).
#define WEBUI     ///< Web control panel (GET /ui, /api/devices, POST
// /api/device).
#define LOBOT ///< Use the LOBOT servo protocol in SerialServoMotorMode.
#define API
#define OTA ///< Enable OTA firmware update (espota + web /update). Optional:
            ///< #define OTA_PASSWORD "…"

// --- WiFi (supprimer les 3 lignes pour désactiver le WiFi) --------------
#include "../auth/wifi.h" // gitignored — définit WIFI_SSID et WIFI_PASSWORD
#define HTTP_PORT 80
// #define AUDIO ///< Not functional yet.
#define DCC_PIN 34
#define OLED
#define OLED_SPLASH
#define LOG_SERIAL