/**
 * @file config.h
 *
 * @brief Configuration for the public browser USB flasher (web-installer/).
 *
 * No compile-time WIFI_SSID/WIFI_PASSWORD — same as every other ESP32
 * profile (#132): a stranger flashes this from a web page with no way to
 * review what's baked in first, so it must never carry credentials at all.
 * Boots straight into its own access point (default SSID/password come from
 * WIFI_AP_SSID/WIFI_AP_PASSWORD, see LayoutFX_define.h) and gets its STA
 * credentials at runtime via the WebUI's WiFi form (POST /api/wifi), stored
 * in /wifi.json on LittleFS — never in the firmware image.
 *
 * Injected into every translation unit via -include in platformio.ini.
 *
 * @project MrJ-LayoutFX-dev
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */
#pragma once

// --- Feature flags ------------------------------------------------------
#define CONFIG "config.json" ///< Config JSON file on LittleFS — starts empty.
#define SPI_CARDS ///< Enable 74HC595 SPI shift-register bus.
#define I2C_CARDS ///< Enable I2C device drivers (PCA9685 servo, etc.).
#define WEBUI     ///< Web control panel (GET /ui, /api/devices, POST /api/device).
#define LOBOT     ///< Use the LOBOT servo protocol in SerialServoMotorMode.
#define API
#define OTA ///< Enable OTA firmware update (espota + web /update) for later updates.

// --- WiFi -----------------------------------------------------------------
// No WIFI_SSID/WIFI_PASSWORD on purpose — see file comment above. The device
// boots straight into AP mode (WIFI_AP_SSID/WIFI_AP_PASSWORD defaults apply).
#define HTTP_PORT 80
#define DCC_PIN 34
#define OLED
#define OLED_SPLASH
#define LOG_SERIAL
