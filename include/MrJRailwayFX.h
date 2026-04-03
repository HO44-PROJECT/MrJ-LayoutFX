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

#ifndef __MRJ_RAILWAY_FX_H__
#define __MRJ_RAILWAY_FX_H__

// ── Audio ────────────────────────────────────────────────────────────────────
#ifdef AUDIO
#include <audio/DfAudio.h>
#endif

// ── DCC ──────────────────────────────────────────────────────────────────────
#ifdef DCC_PIN
#include "dcc/DccCallbacks.h"
#include "dcc/DccDrivable.h"
#endif

// ── Devices ──────────────────────────────────────────────────────────────────
#include "devices/StaticLow.h"
#include "devices/StaticOpen.h"
#include "devices/StaticUp.h"

// ── LED effects ──────────────────────────────────────────────────────────────
#include "led_fx/Beacon.h"
#include "led_fx/CampFire.h"
#include "led_fx/DefectLamp.h"
#include "led_fx/DoubleBeacon.h"
#include "led_fx/ElectricLamp.h"
#include "led_fx/GasLamp.h"
#include "led_fx/Led.h"
#include "led_fx/NeonSign.h"
#include "led_fx/OilLamp.h"
#include "led_fx/RailwayCrossingLights.h"
#include "led_fx/SignalFlare.h"
#include "led_fx/SolderLamp.h"
#include "led_fx/Storm.h"
#include "led_fx/Torch.h"
#include "led_fx/TrainHeadLamp.h"
#include "led_fx/TurnSignal.h"

// ── OLED ─────────────────────────────────────────────────────────────────────
#ifdef OLED
#include "oled/StatusOled.h"
#endif

// ── Servo ────────────────────────────────────────────────────────────────────
#ifdef SERIAL_SERVO
#include "servo/SerialServoMotorMode.h"
#endif

// ── Railway signals ──────────────────────────────────────────────────────────
#include "signals/MrJDbBlocSignal.h"
#include "signals/MrJDbEntrySignal.h"
#include "signals/MrJDbExitSignal.h"

// ── Traffic lights ───────────────────────────────────────────────────────────
#include "traffic/TrafficLight3Phase.h"
#include "traffic/TrafficLight4Phase.h"

// ── Utilities ────────────────────────────────────────────────────────────────
#include "utils/utils.h"

// ── ESP32 networking & config (conditionally compiled) ───────────────────────
#ifdef WEBUI
#include "api/ApiServer.h"
#include "api/WebUI.h"
#endif
#ifdef CONFIG
#include "config/ConfigManager.h"
#endif

// ── Coroutine scheduler (AceRoutine — all platforms) ─────────────────────────
#include <AceRoutine.h>

// ── SPI shift-register bus (optional — requires #define SPI_CARDS) ───────────
#ifdef SPI_CARDS
#include "spi/Spi595Bus.h"
#endif

// ── HTTP port default (user may override in config.h) ────────────────────────
#ifdef ESP32
#ifndef HTTP_PORT
#define HTTP_PORT 80
#endif
#endif

// ── MrJFX — single-call init/loop ────────────────────────────────────────────
/**
 * @class MrJFX
 * @brief Orchestrates the full library initialisation and main loop.
 *
 * All behaviour is selected at compile time via #defines in config.h.
 * No arguments are needed — the user writes:
 *
 *   void setup() { MrJFX::init(); }
 *   void loop()  { MrJFX::loop(); }
 */
class MrJFX
{
public:
        /**
         * @brief Initialise all active subsystems in the correct order:
         *          1. Serial
         *          2. ConfigManager  (if CONFIG is defined — ESP32 only)
         *          3. WebUI          (if WEBUI is defined  — ESP32 only)
         *          4. CoroutineScheduler
         *          5. ApiServer/WiFi (if WIFI_SSID and WIFI_PASSWORD are defined)
         */
        static void init()
        {
                Serial.begin(115200);

#ifdef CONFIG
                // CONFIG holds the filename (without '/') — prepend it for LittleFS.
                ConfigManager::init("/" CONFIG);
#endif

#ifdef WEBUI
                // WebUI must be registered before ApiServer starts the HTTP server.
                WebUI::init(ConfigManager::factory());
#endif

                // Must be called after all coroutines are declared (setup() phase).
                ace_routine::CoroutineScheduler::setup();

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
                // ApiServer connects WiFi and starts the HTTP server on Core 0.
                ApiServer::init(WIFI_SSID, WIFI_PASSWORD, HTTP_PORT);
#endif
        }

        /**
         * @brief Drive all active subsystems each iteration of the Arduino loop:
         *          1. CoroutineScheduler  (all platforms)
         *          2. Spi595Bus::flush()  (if SPI_CARDS is defined)
         */
        static void loop()
        {
                ace_routine::CoroutineScheduler::loop();

#ifdef SPI_CARDS
                // Flush the SPI shift-register output image to hardware.
                Spi595Bus::flush();
#endif

#ifdef DCC_PIN
                // Drive the DCC decoder state machine and callbacks.
                DccDrivable::loop();
#endif
        }
};

#endif // __MRJ_RAILWAY_FX_H__
