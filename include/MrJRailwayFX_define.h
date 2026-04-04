/**
 * @file MrJRailwayFX.h
 * @brief Single entry point for the MrJ-RailwayFX library.
 *
 * Aggregates all public headers and exposes MrJFX::init() / MrJFX::loop()
 * for a minimal user main.cpp:
 *
 *   #include <MrJRailwayFX.h>
 *   #include "config.h"
 *
 *   void setup() { MrJFX::init(); }
 *   void loop()  { MrJFX::loop(); }
 *
 * Behaviour of MrJFX is driven by #defines in the user's config.h:
 *
 *   CONFIG   "file.json"  Load device config from LittleFS (ESP32 only).
 *                         The value is the filename without leading '/'.
 *   WEBUI                 Enable the web control panel (/ui, /api/*).
 *   WIFI_SSID  "…"  \
 *   WIFI_PASSWORD  "…"   Connect to WiFi and start the HTTP server.
 *   HTTP_PORT  <n>        HTTP port — defaults to 80 if not defined.
 *   SPI_CARDS             Enable the 74HC595 SPI shift-register bus.
 *   DEBUG                 Enable DEBUG_PRINT / DEBUG_PRINTLN output.
 *   LOBOT                 Enable the Lobot LX-16A servo protocol.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_default.h>

// ── DCC ──────────────────────────────────────────────────────────────────────
#ifdef DCC_PIN // DCC_PIN must be defined to enable DCC support.
  #define MRJFX_DCC_ENABLED 1
#else
  #undef MRJFX_DCC_ENABLED // DCC is disabled if DCC_PIN is not defined.
#endif                     // End DCC check

// -- WIFI (ESP32 only) ────────────────────────────────────────────────────────
// ── HTTP port default (user may override in config.h) ────────────────────────
#ifdef ESP32        // WiFi is only supported on ESP32, and requires both WIFI_SSID and WIFI_PASSWORD to be defined.
  #ifndef HTTP_PORT // User can define HTTP_PORT in config.h; default to 80 if not defined.
    #define HTTP_PORT 80
  #endif                                           // End HTTP_PORT default
  #if defined(WIFI_SSID) && defined(WIFI_PASSWORD) // Both WIFI_SSID and WIFI_PASSWORD must be defined to enable WiFi.
    #define MRJFX_WIFI_ENABLED 1
  #else
    #undef MRJFX_WIFI_ENABLED // WiFi is disabled if either WIFI_SSID or WIFI_PASSWORD is missing.
  #endif                      // End WIFI check
#endif                        // ESP32

// -- json config file on LittleFS (ESP32 only)
// ─────────────────────────────────────────────
#ifdef ESP32 // CONFIG is the filename (without '/') of the config JSON file onBonjou LittleFS.
  #ifdef CONFIG
    #define MRJFX_CONFIG_ENABLED 1
  #else
    #undef MRJFX_CONFIG_ENABLED
  #endif
#else
  #undef MRJFX_CONFIG_ENABLED
#endif // ESP32

#ifdef ESP32
  #if defined(API) || defined(WEBUI) // API and WEBUI are only supported on ESP32 with WiFi and CONFIG enabled.
    #if !defined(MRJFX_WIFI_ENABLED) || !defined(MRJFX_CONFIG_ENABLED)
      #warning "API or WEBUI requires WIFI and CONFIG to be enabled."
      #undef MRJFX_API_SERVER_ENABLED
    #else
      #ifdef API
        #define MRJFX_API_SERVER_ENABLED 1 // API server is required for the WebUI.
      #endif
      #ifdef WEBUI
        #define MRJFX_WEBUI_ENABLED 1
        #define MRJFX_API_SERVER_ENABLED 1 // API server is required for the WebUI.
      #endif
    #endif // End check for WIFI and CONFIG
  #else
    #undef MRJFX_API_SERVER_ENABLED
  #endif // WEBUI
#else
  #undef MRJFX_API_SERVER_ENABLED // WEBUI is only supported on ESP32.
#endif

#ifdef OLED // OLED support is enabled if OLED is defined (value is ignored).
  #define MRJFX_OLED_ENABLED 1
#else
  #undef MRJFX_OLED_ENABLED
#endif // End OLED check

#ifdef SPI_CARDS // SPI shift-register bus support is enabled if SPI_CARDS is defined.
  #define MRJFX_SPI_CARDS_ENABLED 1
#else
  #undef MRJFX_SPI_CARDS_ENABLED
#endif // End SPI_CARDS check

#ifdef AUDIO // AUDIO support is enabled if AUDIO is defined (value is ignored).
  #define MRJFX_AUDIO_ENABLED 1
#else
  #undef MRJFX_AUDIO_ENABLED
#endif // End AUDIO check

// ── Serial servo support (conditionally compiled) ─────────────────────────────
#if defined(LOBOT) || defined(LX16A) // LOBOT implies LX16A, but user can define LX16A without LOBOT if they want.
  #define MRJFX_SERIAL_SERVO_ENABLED 1
  #if defined(LOBOT)
    #define MRJFX_LOBOT_SERVO_ENABLED 1
  #elif defined(LX16A)
    #define MRJFX_LX16A_SERVO_ENABLED 1
  #endif
#else
  #undef MRJFX_SERIAL_SERVO_ENABLED
#endif // End Serial servo check

// ── Audio ────────────────────────────────────────────────────────────────────
#ifdef MRJFX_AUDIO_ENABLED
  #include <audio/DfAudio.h>
#endif // End of MRJFX_AUDIO_ENABLED includes

// ── DCC support (conditionally compiled) ─────────────────────────────────
#ifdef MRJFX_DCC_ENABLED // DCC support is enabled if DCC_PIN is defined.
  #include "dcc/DccCallbacks.h"
  #include "dcc/DccDrivable.h"
#endif // End DCC check
