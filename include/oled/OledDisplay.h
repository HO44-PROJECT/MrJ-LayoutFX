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

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_OLED_ENABLED

#include <AceRoutine.h>
#include <U8g2lib.h>

/**
 * @class OledDisplay
 * @brief Singleton AceRoutine coroutine managing the structural OLED display.
 *
 * Instantiated once as a global in OledDisplay.cpp. The coroutine registers
 * itself with AceRoutine at construction and is driven by CoroutineScheduler.
 *
 * External code only needs two calls:
 *   OledDisplay::init();                         // in MrJFX::init()
 *   OledDisplay::notify(type, id, state);        // in DeviceApi handlers
 */
class OledDisplay : public ace_routine::Coroutine {
public:
    /**
     * @brief Construct the display and register the coroutine with AceRoutine.
     * Does NOT initialise Wire or U8g2 — call init() for that.
     */
    OledDisplay();

    /**
     * @brief Initialise Wire (I²C) and the U8g2 driver.
     * Must be called once in MrJFX::init(), before CoroutineScheduler::setup().
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
    static void notify(const char* type, const char* id, int state);

    /** @brief AceRoutine entry point — managed by CoroutineScheduler. */
    int runCoroutine() override;

private:
    void _begin();
    void _drawIdle();
    void _drawEvent();
    void _drawIcon(const char* type, uint8_t ox, uint8_t oy);
    static const char* _stateName(const char* type, int state);

    // Event state — shared through the single global instance via static storage.
    static char          _evtType[24];
    static char          _evtId[24];
    static int           _evtState;
    static volatile bool _hasEvent;

    // U8g2 driver — selected at compile time by OLED_HEIGHT.
    // Pins (SCL, SDA) are passed at construction so U8G2 initialises Wire internally.
#if OLED_HEIGHT == 32
    U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C _u8g2;
#else
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C   _u8g2;
#endif
    // Note: constructor is  OledDisplay() : _u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA) {}
};

/** @brief Single global instance — auto-registered with AceRoutine. */
extern OledDisplay oledDisplay;

#endif // MRJFX_OLED_ENABLED
