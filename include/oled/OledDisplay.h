/**
 * @file OledDisplay.h
 * @brief Structural OLED status display for MrJ-RailwayFX — ESP32 + SSD1306 I²C.
 *
 * Drives a 128×64 (or 128×32) SSD1306 display via U8g2 in full-buffer mode.
 * Two screens alternate as an AceRoutine coroutine:
 *
 *   - Idle    : IP address · device count · uptime (refreshed every ~5 s).
 *   - Event   : device icon + type + id + state, shown for OLED_EVENT_MS ms
 *               after each call to OledDisplay::notify().
 *
 * Usage (config.h):
 *   #define OLED                 // enable this module
 *   #define OLED_SDA  21         // optional — default 21
 *   #define OLED_SCL  22         // optional — default 22
 *   #define OLED_HEIGHT  64      // optional — 64 or 32
 *   #define OLED_EVENT_MS 3000   // optional — event screen duration in ms
 *
 * This display is STRUCTURAL (wired once, configured at compile time).
 * It is distinct from I²C devices declared in config.json (type SSD1306).
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_OLED_ENABLED

  #include <U8g2lib.h>

/**
 * @class OledDisplay
 * @brief Singleton managing the structural OLED display on a dedicated Core 0 task.
 *
 * Instantiated once as a global in OledDisplay.cpp. init() starts a FreeRTOS
 * task pinned to Core 0 that handles all blocking I²C transfers (sendBuffer),
 * keeping Core 1 (CoroutineScheduler + PWM effects) unaffected.
 *
 * External code only needs two calls:
 *   OledDisplay::init();                         // in LayoutFX::init()
 *   OledDisplay::notify(type, id, state);        // in DeviceApi handlers
 */
class OledDisplay {
public:
  /**
   * @brief Construct the display instance (does NOT initialise Wire or U8g2).
   */
  OledDisplay();

  /**
   * @brief Initialise the U8g2 driver and start the OLED FreeRTOS task.
   * Wire must already be initialised (Wire.begin called by LayoutFX::init()).
   * Must be called once in LayoutFX::init(), before CoroutineScheduler::setup().
   */
  static void init();

  /**
   * @brief Trigger the event screen for the given device state change.
   * Safe to call from any context (DeviceApi HTTP handlers run on Core 0).
   *
   * @param type  Device type string (value of getDeviceName(), as const char*).
   * @param id    Device id string.
   * @param state New state integer.
   */
  static void notify(const char *type, const char *id, int state);

  /**
   * @brief Draw a two-line message directly to the OLED (synchronous, bypasses event task).
   *
   * Used just before ESP.restart() so the display updates even as the FreeRTOS
   * task is about to be killed. Caller must ensure at least ~50 ms before restart.
   *
   * @param line1 First line text.
   * @param line2 Second line text (may be nullptr).
   */
  static void showMessage(const char *line1, const char *line2 = nullptr);

  /**
   * @brief Post a one-line log message to the OLED (non-blocking, via event pipe).
   *
   * Displayed as a transient screen between idle and device-event screens.
   * On ESP32, F() strings can be passed directly (cast to const char*).
   *
   * @param msg  Log message (SRAM string, max 21 chars visible at 6×10 font).
   */
  static void log(const char *msg);
  static void log(const __FlashStringHelper *msg);

  /**
   * @brief Store the active configuration name for display on the idle screen.
   * Called by ConfigManager after every load / hot-reload.
   * Pass an empty string or nullptr to fall back to LFX_PROJECT_NAME.
   */
  static void setConfigName(const char *name);

  /**
   * @brief Switch the OLED to a persistent "SAFE MODE" screen (config bypassed).
   * Called once at boot when SafeMode is active; the render task keeps it shown.
   */
  static void setSafeMode();

private:
  bool _begin(); ///< Returns false if no display ACKs on the I²C bus — suppresses task creation.
  void _drawIdle();
  void _drawSafeMode();
  void _drawEvent();
  void _drawLog();
  void _drawIcon(const char *type, uint8_t ox, uint8_t oy);
  static const char *_stateName(const char *type, int state);
  static void _task(void *);
  #ifdef LFX_OLED_SPLASH_ENABLED
  void _drawSplash();
  void _drawTrain(int tx, int frame);

  static constexpr int kSplashStepPx = 3;      ///< Pixels per frame.
  static constexpr int kSplashDelayMs = 40;    ///< Ms per frame (~25 fps).
  static constexpr int kSplashWidthPx = 96;    ///< Full train width (px).
  static constexpr int kSplashSpokeFrames = 3; ///< Frames per spoke orientation.
  #endif

  // Config name shown on idle screen (set by ConfigManager via setConfigName()).
  static char _configName[32];

  // Event state — shared through the single global instance via static storage.
  static char _evtType[24];
  static char _evtId[24];
  static int _evtState;
  static volatile bool _hasEvent;

  // Log state
  static char _logMsg[44];
  static volatile bool _hasLog;

  // Safe-mode screen (config bypassed) — persistent until reboot.
  static volatile bool _safeMode;

  // U8g2 driver — selected at compile time by OLED_HEIGHT.
  // Pins (SCL, SDA) are passed at construction so U8G2 initialises Wire internally.
  #if OLED_HEIGHT == 32
  U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C _u8g2;
  #else
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C _u8g2;
  #endif
  // Note: constructor uses U8X8_PIN_NONE for clock+data — Wire is pre-initialized
  //       by LayoutFX::init(); passing pins would re-call Wire.begin() (breaking arduino-esp32 v3).
};

/** @brief Single global instance — auto-registered with AceRoutine. */
extern OledDisplay oledDisplay;

#endif // LFX_OLED_ENABLED
