/**
 * @file MrJRailwayFX_define.h
 * @brief Feature-gate and size-limit defines for the MrJ-RailwayFX library.
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
 * ───────────────────────────────────────────────────────────────────────────
 * USER FLAGS — every #define the user may set in config.h. Presence enables the
 * feature unless a <value> is shown. Each maps to an internal MRJFX_*_ENABLED
 * macro below, or is consumed directly where noted. KEEP THIS LIST IN SYNC with
 * the WebUI feature badges (DeviceStatusApi.cpp `feat[...]`).
 * Toggle flags default to OFF (undefined); value flags show their default.
 * Full reference with every default → doc/configuration-flags.md.
 * ───────────────────────────────────────────────────────────────────────────
 *
 * Core / config / network (ESP32 only):
 *   CONFIG "file.json"    Load the device config from LittleFS (filename, no '/').
 *   API                   REST API server (/api/…). Requires WIFI + CONFIG.
 *   WEBUI                 Web control panel (/ui). Implies API. Requires WIFI + CONFIG.
 *   WIFI_SSID "…"     \   STA credentials — BOTH required to join WiFi and start
 *   WIFI_PASSWORD "…" /   the HTTP server.
 *   WIFI_AP_SSID "…"      AP-fallback SSID     (default: MRJFX_PROJECT_NAME).
 *   WIFI_AP_PASSWORD "…"  AP-fallback password (default "layoutfx1234", min 8 chars).
 *   WIFI_FORCE_AP         Skip STA entirely, boot straight into access-point mode.
 *   HTTP_PORT <n>         HTTP port (default 80).
 *   OTA                   Wireless firmware update: espota (pio upload) + web /update.
 *   OTA_HOSTNAME "…"      mDNS name prefix (default "mrjfx") → "<name>-<MAC>".
 *   OTA_PASSWORD "…"      Optional auth for espota and the web uploader.
 *
 * Buses / hardware:
 *   DCC_PIN <n>           NMRA DCC decoder on GPIO <n> (its presence enables DCC).
 *   DCC_AUDIT             Log every received DCC packet (diagnostics).
 *   SPI_CARDS             74HC595 SPI shift-register bus (chained digital outputs).
 *   I2C_CARDS             I²C device drivers (PCA9685 servo boards…); brings up I²C.
 *   I2C_SCAN              Expose GET /api/scan/i2c; also brings up the I²C bus.
 *   I2C_SDA <n> / I2C_SCL <n>   I²C bus pins (default 21 / 22).
 *   LOBOT                 Lobot LX-16A serial-servo protocol (implies LX16A).
 *   LX16A                 LX-16A serial servo without the full Lobot stack.
 *   AUDIO                 DFPlayer-style serial audio device support.
 *   USE_JTAG              Do NOT drive GPIO 5/10/12-15 LOW at boot (keep JTAG usable).
 *
 * OLED display:
 *   OLED                  SSD1306 OLED over I²C (brings up the I²C bus).
 *   OLED_STATUS           Lightweight status screen via StatusOled (also on AVR).
 *   OLED_SPLASH           Show a boot splash screen.
 *   OLED_CONTRAST <n>     Contrast 0-255 (default: display default, not applied).
 *   OLED_FLIP_MODE <n>    Rotation / flip mode (default: no flip).
 *   OLED_HEIGHT <64|32>   Panel height in px (default 64; 32 for a 0.91" panel).
 *   OLED_SDA <n> / OLED_SCL <n>   OLED on a separate bus (default = I2C_SDA / I2C_SCL).
 *   OLED_EVENT_MS <n>     Event-screen display time (default 3000 ms).
 *   OLED_DEBUG_METRICS    Show runtime metrics (heap, uptime…) on the OLED.
 *   OLED_DEBUG_EVENTS     Show event traces on the OLED.
 *
 * Logging — independent sinks, NOT mutually exclusive. Three serial situations:
 *   (boot default)        Serial.begin() at boot prints STRUCTURAL Tier-1 logs
 *                         (banner, IP, config). Always on — NOT config-toggleable.
 *   LOG_SERIAL            Operational Tier-2 logs on UART0 (LOG_PRINT…). Runtime-
 *                         gated by the uart0 bus; remove it to free GPIO1/3.
 *   DEBUG_SERIAL          Verbose debug logs on UART0 (DEBUG_PRINT…).
 *   LOG_OLED              Mirror operational logs to the OLED.
 *   DEBUG_OLED            Send debug logs to the OLED instead of serial.
 *
 * Behaviour:
 *   SERVO_PRESERVE_DIRECTION  Servo keeps its last travel direction across moves.
 *   DEMO                  Built-in demo sequences (traffic, signals, servo, LED FX).
 *
 * Capacity limits (override the default shown):
 *   BUS_MAX_SPI_CARDS 8    BUS_MAX_UART 4    BUS_MAX_I2C 2
 *   FACTORY_MAX_DEVICES 256    FACTORY_MAX_BOARDS 12    FACTORY_MAX_BUSES 8
 *   FACTORY_MAX_PORTS 4    FACTORY_MAX_BOARD_TYPES 16    FACTORY_MAX_SPI_CARDS 8
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
  #ifdef DCC_AUDIT
    #define MRJFX_DCC_AUDIT_ENABLED 1
  #else
    #undef MRJFX_DCC_AUDIT_ENABLED
  #endif
#else
  #undef MRJFX_DCC_ENABLED // DCC is disabled if DCC_PIN is not defined.
  #undef MRJFX_DCC_AUDIT_ENABLED
#endif // End DCC check

// -- WIFI (ESP32 only) ────────────────────────────────────────────────────────
// ── HTTP port default (user may override in config.h) ────────────────────────
#ifdef ESP32 // WiFi is only supported on ESP32, and requires both WIFI_SSID and WIFI_PASSWORD to be defined.
  #ifdef HTTP_PORT
    #define MRJFX_API_HTTP_PORT HTTP_PORT
  #else
    #define MRJFX_API_HTTP_PORT 80
  #endif
  #if defined(WIFI_SSID) && defined(WIFI_PASSWORD) // Both WIFI_SSID and WIFI_PASSWORD must be defined to enable WiFi.
    #define MRJFX_WIFI_ENABLED 1
  #else
    #undef MRJFX_WIFI_ENABLED // WiFi is disabled if either WIFI_SSID or WIFI_PASSWORD is missing.
  #endif                      // End WIFI check
  // AP fallback credentials — user may override in wifi.h / config.h.
  #ifndef WIFI_AP_SSID
    #define WIFI_AP_SSID MRJFX_PROJECT_NAME
  #endif
  #ifndef WIFI_AP_PASSWORD
    #define WIFI_AP_PASSWORD "layoutfx1234"
  #endif
  // Define WIFI_FORCE_AP in config.h to skip STA entirely and start in AP mode.
  #ifdef WIFI_FORCE_AP
    #define MRJFX_WIFI_FORCE_AP 1
  #endif
#endif // ESP32

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

#if defined(MRJFX_OLED_ENABLED) && defined(OLED_SPLASH)
  #define MRJFX_OLED_SPLASH_ENABLED 1
#else
  #undef MRJFX_OLED_SPLASH_ENABLED
#endif

// ── I²C bus (ESP32 only) ──────────────────────────────────────────────────────
// Activated by any of: #define I2C_CARDS, #define I2C_SCAN, #define OLED.
// Drives Wire.begin() in MrJFX::init() via MRJFX_I2C_CARDS_ENABLED.
#if defined(ESP32) && (defined(I2C_CARDS) || defined(I2C_SCAN) || defined(OLED))
  #define MRJFX_I2C_CARDS_ENABLED 1
#else
  #undef MRJFX_I2C_CARDS_ENABLED
#endif

// ── Boot-sensitive pin release (ESP32 only) ───────────────────────────────────
// GPIO 5, 10, 12-15 are driven LOW at startup by default (strapping + JTAG pins).
// Define USE_JTAG in config.h to skip this entirely (e.g. when using a JTAG probe).
#if defined(ESP32) && !defined(USE_JTAG)
  #define MRJFX_RELEASE_JTAG 1
#else
  #undef MRJFX_RELEASE_JTAG
#endif

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

// ── I2C scanner (ESP32 only) ──────────────────────────────────────────────────
// GET /api/scan/i2c (web UI "Scan I2C" button) is available as soon as the I2C
// bus is up — it only walks addresses on an already-initialised Wire bus, so it
// has no dependency of its own. Enabled by I2C_CARDS, OLED or an explicit
// I2C_SCAN (all three drive MRJFX_I2C_CARDS_ENABLED above).
#if defined(MRJFX_I2C_CARDS_ENABLED)
  #define MRJFX_I2C_SCAN_ENABLED 1
#else
  #undef MRJFX_I2C_SCAN_ENABLED
#endif

// ── I2C device drivers (ESP32 only) ──────────────────────────────────────────
// Define I2C_CARDS in config.h to enable I2C-based board device drivers (PCA9685 servo, etc.)
#if defined(ESP32) && defined(I2C_CARDS)
  #define MRJFX_I2C_DEVICES_ENABLED 1
#else
  #undef MRJFX_I2C_DEVICES_ENABLED
#endif

// ── OTA firmware update (ESP32 only) ──────────────────────────────────────────
// Define OTA in config.h to enable wireless firmware updates, both:
//   • ArduinoOTA / espota  → `pio run -t upload` over WiFi (from a dev machine)
//   • web endpoint /update → upload a .bin from a browser (field updates, no PC)
// Needs WiFi (brought up by the API server) — enable alongside WEBUI / API.
#if defined(ESP32) && defined(OTA)
  #define MRJFX_OTA_ENABLED 1
#else
  #undef MRJFX_OTA_ENABLED
#endif

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

// ── Bus registry limits ───────────────────────────────────────────────────
#ifdef BUS_MAX_SPI_CARDS
  #define MRJFX_BUS_MAX_SPI_CARDS BUS_MAX_SPI_CARDS
#else
  #define MRJFX_BUS_MAX_SPI_CARDS 8
#endif

#ifdef BUS_MAX_UART
  #define MRJFX_BUS_MAX_UART BUS_MAX_UART
#else
  #define MRJFX_BUS_MAX_UART 4
#endif

#ifdef BUS_MAX_I2C
  #define MRJFX_BUS_MAX_I2C BUS_MAX_I2C
#else
  #define MRJFX_BUS_MAX_I2C 2
#endif

// ── Max simultaneous devices ──────────────────────────────────────────────
#ifdef FACTORY_MAX_DEVICES
  #define MRJFX_FACTORY_MAX_DEVICES FACTORY_MAX_DEVICES
#else
  #define MRJFX_FACTORY_MAX_DEVICES 256
#endif

// ── DeviceFactory structure limits ───────────────────────────────────────
#ifdef FACTORY_MAX_BOARDS
  #define MRJFX_FACTORY_MAX_BOARDS FACTORY_MAX_BOARDS
#else
  #define MRJFX_FACTORY_MAX_BOARDS 12
#endif

#ifdef FACTORY_MAX_BUSES
  #define MRJFX_FACTORY_MAX_BUSES FACTORY_MAX_BUSES
#else
  #define MRJFX_FACTORY_MAX_BUSES 8
#endif

#ifdef FACTORY_MAX_PORTS
  #define MRJFX_FACTORY_MAX_PORTS FACTORY_MAX_PORTS
#else
  #define MRJFX_FACTORY_MAX_PORTS 4
#endif

#ifdef FACTORY_MAX_BOARD_TYPES
  #define MRJFX_FACTORY_MAX_BOARD_TYPES FACTORY_MAX_BOARD_TYPES
#else
  #define MRJFX_FACTORY_MAX_BOARD_TYPES 16
#endif

#ifdef FACTORY_MAX_SPI_CARDS
  #define MRJFX_FACTORY_MAX_SPI_CARDS FACTORY_MAX_SPI_CARDS
#else
  #define MRJFX_FACTORY_MAX_SPI_CARDS 8
#endif
