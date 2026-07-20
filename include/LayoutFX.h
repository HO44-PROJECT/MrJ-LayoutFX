/**
 * @file LayoutFX.h
 * @brief Single entry point for the MrJ-LayoutFX library.
 *
 * Aggregates all public headers and exposes LayoutFX::init() / LayoutFX::loop()
 * for a minimal user main.cpp:
 *
 *   #include <LayoutFX.h>
 *   #include "config.h"
 *
 *   void setup() { LayoutFX::init(); }
 *   void loop()  { LayoutFX::loop(); }
 *
 * Behaviour of LayoutFX is driven by #defines in the user's config.h:
 *
 *   CONFIG   "file.json"  Load device config from LittleFS (ESP32 only).
 *                         The value is the filename without leading '/'.
 *   WEBUI                 Enable the web control panel (/ui, /api/).
 *   WIFI_SSID  "…"  \
 *   WIFI_PASSWORD  "…"   Connect to WiFi and start the HTTP server.
 *   HTTP_PORT  <n>        HTTP port — defaults to 80 if not defined.
 *   SPI_CARDS             Enable the 74HC595 SPI shift-register bus.
 *   DEBUG                 Enable MRJ_DEBUG_PRINT / MRJ_DEBUG_PRINTLN output.
 *   LOBOT                 Enable the Lobot LX-16A servo protocol.
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <LayoutFX_define.h>

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
#include "led_fx/GasLampDefect.h"
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

// ── I²C bus ───────────────────────────────────────────────────────────────────
#ifdef LFX_I2C_CARDS_ENABLED
  #include <Wire.h>
#endif

// ── OLED ─────────────────────────────────────────────────────────────────────
#ifdef LFX_OLED_ENABLED
  #include "oled/OledDisplay.h"
#endif

// ── Servo ────────────────────────────────────────────────────────────────────
#ifdef LFX_SERIAL_SERVO_ENABLED
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
#ifdef LFX_CONFIG_ENABLED
  #include "bus/BusRegistry.h"
  #include "config/ConfigManager.h"
  #include "utils/SafeMode.h"
#endif

#ifdef LFX_API_SERVER_ENABLED
  #include "api/ApiServer.h"
  #include "api/DeviceApi.h"
  #include "api/Identify.h"
  #include "api/WifiApi.h"
#endif

#ifdef LFX_WEBUI_ENABLED
  #include "api/WebUI.h"
#endif

#ifdef LFX_OTA_ENABLED
  #include "api/OtaUpdater.h"
#endif

// ── Coroutine scheduler (AceRoutine — all platforms) ─────────────────────────
#include <AceRoutine.h>

// ── SPI shift-register bus (optional — requires #define SPI_CARDS) ───────────
#ifdef LFX_SPI_CARDS_ENABLED
  #include "spi/Spi595Bus.h"
#endif // End LFX_SPI_CARDS_ENABLED includes

// ── Audio ────────────────────────────────────────────────────────────────────
#ifdef LFX_AUDIO_ENABLED
  #include <audio/DfAudio.h>
#endif // End of LFX_AUDIO_ENABLED includes

// ── DCC support (conditionally compiled) ─────────────────────────────────
#ifdef LFX_DCC_ENABLED // DCC support is enabled if DCC_PIN is defined.
  #include "dcc/DccCallbacks.h"
  #include "dcc/DccDrivable.h"
#endif // End DCC check

// ── JTAG release ─────────────────────────────────────────────────────────────
// No driver/gpio.h needed — we use Arduino pinMode/digitalWrite.

// ── LayoutFX — single-call init/loop ────────────────────────────────────────────
/**
 * @class LayoutFX
 * @brief Orchestrates the full library initialisation and main loop.
 *
 * All behaviour is selected at compile time via #defines in config.h.
 * No arguments are needed — the user writes:
 *
 *   void setup() { LayoutFX::init(); }
 *   void loop()  { LayoutFX::loop(); }
 */
class LayoutFX {
public:
  /**
   * @brief Initialise all active subsystems in the correct order:
   *          1. Serial
   *          2. ConfigManager  (if CONFIG is defined — ESP32 only)
   *          3. WebUI          (if WEBUI is defined  — ESP32 only)
   *          4. CoroutineScheduler
   *          5. ApiServer/WiFi (if WIFI_SSID and WIFI_PASSWORD are defined)
   */
  static void init() {
    // 0a. Drive boot-sensitive pins LOW — prevents LED flicker on JTAG/strapping pins.
    //     Uses Arduino pinMode/digitalWrite (no ESP-IDF driver dependency).
    //     Define USE_JTAG in config.h to skip this block.
#ifdef LFX_RELEASE_JTAG
    { const uint8_t _p[] = {5, 12, 13, 14, 15};
      for (uint8_t p : _p) { pinMode(p, OUTPUT); digitalWrite(p, LOW); } }
#endif

#if defined(LOG_SERIAL) || defined(MRJ_DEBUG_SERIAL)
    Serial.begin(115200);
#endif

    // 0a-bis. Double-reset recovery. Two quick resets / power-cycles (bulb-style)
    //         latch "safe mode" for this session: config is NOT loaded (no devices,
    //         GPIO/UART untouched) and WiFi is forced to the SoftAP, so the WebUI is
    //         always reachable to repair a config that crashes or locks you out.
    //         Non-persistent — the next normal boot loads the config again.
#ifdef LFX_CONFIG_ENABLED
    SafeMode::begin();
    if (SafeMode::active())
      Serial.println(F("[SafeMode] double-reset → config bypassed this session (devices/buses skipped)"));
#endif

    // StatusOled lightweight display (AVR) — initialises Wire + display.
    // Applies config (contrast, flip mode) and shows splash screen.
    // No-op when OLED_STATUS is not defined.
    STATUS_INIT();

    // 0. Initialise I²C bus (OLED, I2C_CARDS, I2C_SCAN all depend on it).
#ifdef LFX_I2C_CARDS_ENABLED
    Wire.begin(I2C_SDA, I2C_SCL);
    BusRegistry::preInitI2c(); // prevent activateI2c() from calling Wire.begin() again
    // I2C General Call Soft Reset (addr=0x00, cmd=0x06): resets all PCA9685 chips to
    // power-on state (all registers=0, outputs OFF) as early as possible.
    // Prevents spurious servo movement during boot when the ESP32 resets without
    // cutting power to the PCA9685 (which would otherwise keep emitting old PWM values).
    // Other I2C devices (OLED SSD1306, etc.) ignore this command.
#ifdef LFX_I2C_DEVICES_ENABLED
    Wire.beginTransmission(0x00);
    Wire.write(0x06);
    Wire.endTransmission();
#endif
#endif

    // 0b. Start OLED display early (shows boot context).
#ifdef LFX_OLED_ENABLED
    OledDisplay::init();
  #ifdef LFX_CONFIG_ENABLED
    if (SafeMode::active()) OledDisplay::setSafeMode(); // persistent "SAFE MODE" screen
  #endif
#endif

    // 1. Load config from LittleFS and init devices — device/DCC init is SKIPPED
    //    in safe mode so a broken config can never re-crash the boot, but LittleFS
    //    itself must still be mounted (mountFs(), no device load): /api/config*
    //    (DeviceApi) dereferences configPath() unconditionally once registered
    //    below, and WifiApi's /wifi.json read/write — the double-reset "change
    //    WiFi" flow this safe-mode AP exists for, #132 — needs the filesystem too.
    // NOTE: if the uart0 log bus is off, ConfigManager::init() closes UART0 itself,
    // BETWEEN parsing the config and calling initAll() — see the comment there for
    // why that ordering (not here, not at the end of this function) is required.
#ifdef LFX_CONFIG_ENABLED
    if (SafeMode::active())
      ConfigManager::mountFs("/" CONFIG);
    else
      ConfigManager::init("/" CONFIG);
#endif

    // 1b. First-boot hint on OLED when config is empty (no devices configured).
#if defined(LFX_OLED_ENABLED) && defined(LFX_CONFIG_ENABLED)
    if (ConfigManager::factory().count() == 0)
      OledDisplay::log("No config — use WebUI");
#endif

    // 2. Register /api/* routes (requires config to be loaded first).
#ifdef LFX_API_SERVER_ENABLED
    DeviceApi::init(ConfigManager::factory());
    WifiApi::init();
#endif

    // 3. Register /ui route (HTML page — requires DeviceApi routes to be up).
#ifdef LFX_WEBUI_ENABLED
    WebUI::init();
#endif

    // 4. Start coroutine scheduler (after all devices are registered).
    ace_routine::CoroutineScheduler::setup();

    // 5. Connect WiFi and start HTTP server on Core 0.
#ifdef LFX_API_SERVER_ENABLED
  #ifdef LFX_OTA_ENABLED
    // Register /update on the WebServer before it starts.
    OtaUpdater::registerWebRoutes();
  #endif
    // STA credentials saved at runtime via POST /api/wifi (WifiApi, #132) take
    // priority over compile-time WIFI_SSID/WIFI_PASSWORD — e.g. a web_installer
    // build (no compiled-in creds) that's been provisioned by its owner. Falls
    // back to the compile-time values, then to ApiServer's own STA→AP fallback
    // if neither works or neither is set. Skipped in safe mode: LittleFS was
    // never mounted (ConfigManager::init() was skipped above).
    //
    // Safe mode forces the AP directly (forceAp below) — this is also the
    // user-facing way to reach the WiFi form on a device that's already happily
    // joined to STA (nominal "I want to change networks" flow, #132): double-reset
    // → guaranteed AP → WiFi gate. No separate always-on WebUI screen/auth needed.
    String _staSsid = WIFI_SSID, _staPassword = WIFI_PASSWORD;
  #ifdef LFX_CONFIG_ENABLED
    if (!SafeMode::active())
      WifiApi::load(_staSsid, _staPassword);
  #endif
    ApiServer::init(_staSsid.c_str(), _staPassword.c_str(), WIFI_AP_SSID, WIFI_AP_PASSWORD, LFX_API_HTTP_PORT,
  #ifdef LFX_CONFIG_ENABLED
                     SafeMode::active());
  #else
                     false);
  #endif
  #ifdef LFX_OTA_ENABLED
    // ArduinoOTA (espota) + mDNS — after WiFi is up (works in STA and AP).
    OtaUpdater::beginArduinoOta();
  #endif
#endif
  }

  /**
   * @brief Drive all active subsystems each iteration of the Arduino loop:
   *          1. CoroutineScheduler  (all platforms)
   *          2. Spi595Bus::flush()  (if LFX_SPI_CARDS_ENABLED is defined)
   *          3. StatusOled::loop()  (if OLED_STATUS is defined)
   */
  static void loop() {
    ace_routine::CoroutineScheduler::loop();

#ifdef LFX_OTA_ENABLED
    // Poll ArduinoOTA (no-op unless an espota push is in progress).
    OtaUpdater::handle();
#endif

#ifdef LFX_API_SERVER_ENABLED
    // Advance the LED identify blinker (no-op unless a target is active).
    // Before BusRegistry::flush() so an SPI target is propagated this iteration.
    Identify::loop();
#endif

#ifdef LFX_CONFIG_ENABLED
    SafeMode::loop(); // clears the reset counter once past the multi-tap window
    ConfigManager::handlePendingReload();
    BusRegistry::flush();
#endif

#if LFX_DCC_ENABLED
    // Drive the DCC decoder state machine and callbacks.
    DccDrivable::loop();
#endif

    // StatusOled auto-update (metrics, event timeouts, etc.)
    STATUS_LOOP();
  }
};
