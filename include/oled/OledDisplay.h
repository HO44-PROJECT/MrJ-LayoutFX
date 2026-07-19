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
 *   #define OLED_HEIGHT  64      // optional — default/fallback height, 64 or 32
 *   #define OLED_EVENT_MS 3000   // optional — event screen duration in ms
 *
 * This display is STRUCTURAL (wired once), but its presence and resolution
 * are now config-driven (#51): ConfigManager declares an "SSD1306" board in
 * config.json (with an optional "oled_height": 32|64) and calls
 * OledDisplay::configure() after every load/hot-reload — so plugging a
 * different panel and updating the config adapts the rendering without a
 * reboot. OLED_HEIGHT is only the fallback used before the first config load.
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
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

  /**
   * @brief Apply the config-declared presence/resolution of the SSD1306 board (#51).
   *
   * Called by ConfigManager after every init() / reload() / handlePendingReload(),
   * mirroring setConfigName(). Idempotent — safe to call every time even when
   * nothing changed. Only records the request (_present/_pendingHeight): the
   * actual U8G2 re-configuration happens inside the Core 0 render task, which
   * is the sole owner of _u8g2 — this call may run on Core 1 (ConfigManager),
   * so it must never touch _u8g2 directly.
   *
   * The FreeRTOS render task always runs once the physical display ACKs on I²C
   * at boot (probed by init(), independent of config) — this call only gates
   * what it draws:
   *   - present == false: render loop shows a blank screen; notify()/log() become
   *     effective no-ops (their state is still recorded but never drawn).
   *   - present == true: if @p height differs from the currently active height
   *     (or the display had been blanked), the render task re-configures the
   *     U8G2 driver at runtime for the new panel size on its next iteration —
   *     no reboot, no dual-driver compilation (both SSD1306 sizes share the
   *     same U8G2 base class and differ only by which u8g2_Setup_ssd1306_i2c_*
   *     function initialises them).
   * No-op (besides recording _active=false) if init() never found a display.
   *
   * @param present Whether an "SSD1306" board is declared in the loaded config.
   * @param height  Declared panel height in px (32 or 64). Ignored when !present.
   */
  static void configure(bool present, uint8_t height);

private:
  bool _begin(); ///< Returns false if no display ACKs on the I²C bus — suppresses task creation.
  void _applySize(uint8_t height); ///< Re-run the U8G2 setup function for the given panel height. Core 0 (_task) only.
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

  // Scratch buffer for _stateName()'s formatted-speed case (servos/motors have
  // a continuous value, not a fixed label in STATE_LABELS) — Core 0 only, safe
  // to share since _drawEvent() is the sole caller (#46).
  static char _stateNameBuf[8];

  // Safe-mode screen (config bypassed) — persistent until reboot.
  static volatile bool _safeMode;

  // Presence/resolution, driven by ConfigManager via configure() (#51).
  // _height gates every draw function's layout at runtime (replaces the old
  // compile-time #if OLED_HEIGHT branches) and is only ever written by the
  // Core 0 render task (_task/_applySize) — never by configure() itself,
  // which may run on Core 1 and must not touch _u8g2 or _height directly.
  static volatile bool _found;   ///< True once init()'s I²C probe ACKs (physical presence).
  static volatile bool _active;  ///< True when _found AND the config currently declares the board present.
  static volatile bool _present; ///< Latest request from configure() — consumed by _task().
  static volatile uint8_t _pendingHeight; ///< Latest requested height from configure() — consumed by _task().
  static uint8_t _height; ///< Currently active height — Core 0 (_task) only.

  // U8g2 driver — a single instance re-configured at runtime by _applySize()
  // via the u8g2_Setup_ssd1306_i2c_* function, instead of picking one of two
  // compiled subclasses. Both SSD1306 sizes share the same U8G2 base class and
  // differ only by which one-line C setup function initialises the u8g2_t.
  // Pins (SCL, SDA) are passed as U8X8_PIN_NONE — Wire is pre-initialized by
  // LayoutFX::init(); passing pins would re-call Wire.begin() (breaks arduino-esp32 v3).
  U8G2 _u8g2;
};

/** @brief Single global instance — auto-registered with AceRoutine. */
extern OledDisplay oledDisplay;

#endif // LFX_OLED_ENABLED
