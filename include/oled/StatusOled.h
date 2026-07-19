/**
 * @file StatusOled.h
 * @brief Defines the `StatusOled` class for managing a 128x32 OLED display.
 *
 * This header file provides an interface for controlling a 128x32 OLED display
 * using the U8g2 library over I2C. It supports printing text to line 3 and updating
 * status indicators on lines 1 and 2. A single shared buffer minimizes SRAM usage,
 * and unified print methods reduce code size. Designed for Arduino Nano in the
 * MrJ-LayoutFX project.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-09-14
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#ifdef OLED_STATUS

#include <U8g2lib.h>
#include <Wire.h>
#include <Arduino.h>

class StatusOled {
private:
    U8X8_SSD1306_128X32_UNIVISION_HW_I2C oled; // Hardware I2C OLED display
    char buffer[12] = {0}; // Shared buffer for string conversions (max int32: -2147483648)
    uint8_t cursorCol; // Current cursor column for line 3

    // Internal state tracking
    uint8_t deviceStates[16] = {0};      // Track device states (max 16 devices)
    uint8_t deviceCount = 0;             // Number of registered devices
    unsigned long lastMetricsUpdate = 0; // Last metrics update timestamp
    unsigned long eventDisplayEnd = 0;   // Event display end time
    bool inEventMode = false;            // Currently showing event details

    // Internal method to print text to line 3 and update cursor
    void printInternal(const char *text, bool newLine);

    // Internal update methods
    void updateNormalDisplay();
    void restoreNormalDisplay();

public:
    /// Constructor
    StatusOled();

    /// Initialize OLED display and I2C
    void begin();

    /// Update status on lines 1 and 2 for given ID and value
    void updateVisibleStatus(uint8_t id, char value);

    void label(uint8_t id, char value);

    /// Print SRAM string to line 3
    void print(const char *text, bool newLine = false);

    /// Print flash string (F()) to line 3
    void print(const __FlashStringHelper *text, bool newLine = false);

    /// Print 32-bit integer to line 3
    void print(int value, bool newLine = false);

    /// Print 8-bit unsigned integer to line 3
    void print(uint8_t value, bool newLine = false);

    /**
     * @brief Show a two-line splash screen and clear after delayMs.
     * @param name    Project name (SRAM or F() string).
     * @param version Version string (SRAM or F() string).
     * @param delayMs How long to show the splash (ms, default 1500).
     */
    void showSplash(const __FlashStringHelper *name,
                    const __FlashStringHelper *version,
                    uint16_t delayMs = 1500);

    /**
     * @brief Write a fixed info message to row 2.
     * Overwrites the full row to clear previous content.
     * @param text SRAM string (max 16 chars).
     */
    void info(const char *text);

    /// info() overload accepting F() strings.
    void info(const __FlashStringHelper *text);

    /**
     * @brief Clear a specific line (0-3).
     * @param line Line number to clear (0-3).
     */
    void clearLine(uint8_t line);

    /**
     * @brief Print text at a specific position without affecting cursor.
     * @param col Column position (0-15).
     * @param row Row position (0-3).
     * @param text Text to print (SRAM string).
     */
    void printAt(uint8_t col, uint8_t row, const char *text);

    /// printAt() overload accepting F() strings.
    void printAt(uint8_t col, uint8_t row, const __FlashStringHelper *text);

    /**
     * @brief Set display contrast level.
     * @param level Contrast level (0-255, default 128).
     */
    void setContrast(uint8_t level);

    /**
     * @brief Draw a progress bar on the specified row.
     * @param row Row position (0-3).
     * @param value Current value.
     * @param max Maximum value.
     */
    void drawBar(uint8_t row, uint8_t value, uint8_t max);

    /**
     * @brief Enable or disable inverse video mode.
     * @param enabled True to invert (white on black becomes black on white).
     */
    void inverse(bool enabled);

    /**
     * @brief Enable or disable power save mode (screen off).
     * @param enabled True to turn off display, false to turn on.
     */
    void powerSave(bool enabled);

    /**
     * @brief Flip display orientation 180 degrees.
     * @param flip True to flip, false for normal orientation.
     */
    void setFlipMode(bool flip);

    // ── High-level helpers ────────────────────────────────────────────────────

    /**
     * @brief Show device event details temporarily.
     * @param deviceId Device identifier (max 15 chars).
     * @param state State name or value (max 15 chars).
     * @param value Optional numeric value (e.g., PWM level, 0-255).
     */
    void showEvent(const char *deviceId, const char *state, uint8_t value = 0);

    /// showEvent() overload accepting F() strings.
    void showEvent(const __FlashStringHelper *deviceId, const __FlashStringHelper *state, uint8_t value = 0);

    /**
     * @brief Display system metrics (RAM, uptime, device count).
     * @param freeRam Free RAM in bytes.
     * @param deviceCount Number of active devices.
     */
    void showMetrics(uint16_t freeRam, uint8_t deviceCount = 0);

    /**
     * @brief Flash an error message with inverse video.
     * Overwrites entire display. Call repeatedly in loop to maintain flash effect.
     * @param message Error message (max 15 chars per line).
     * @param flashState Toggle this boolean each call to create flash effect.
     */
    void flashError(const char *message, bool flashState);

    /// flashError() overload accepting F() strings.
    void flashError(const __FlashStringHelper *message, bool flashState);

    /**
     * @brief Get free RAM (ATmega only).
     * @return Free RAM in bytes.
     */
    static uint16_t getFreeRam();

    // ── Auto-update system ────────────────────────────────────────────────────

    /**
     * @brief Register a device for auto-tracking.
     * @param id Device ID (0-15).
     * @param label Character label to display on line 0.
     */
    void registerDevice(uint8_t id, char label);

    /**
     * @brief Update device state and refresh display automatically.
     * @param id Device ID (0-15).
     * @param state New state (0=OFF, non-zero=ON).
     * @param showDetailedEvent Show full event screen if state changed (default: false).
     */
    void updateDevice(uint8_t id, uint8_t state, bool showDetailedEvent = false);

    /**
     * @brief Main loop update — call this in loop() to auto-refresh display.
     * Handles metrics updates, event timeouts, etc.
     */
    void loop();
};

#endif // OLED_STATUS
