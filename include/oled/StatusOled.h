/**
 * @file StatusOled.h
 * @brief Defines the `StatusOled` class for managing a 128x32 OLED display.
 *
 * This header file provides an interface for controlling a 128x32 OLED display
 * using the U8g2 library over I2C. It supports printing text to line 3 and updating
 * status indicators on lines 1 and 2. A single shared buffer minimizes SRAM usage,
 * and unified print methods reduce code size. Designed for Arduino Nano in the
 * MrJ-ArduinoRailwayFX project.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-09-14
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include <U8g2lib.h>
#include <Wire.h>
#include <Arduino.h>

class StatusOled {
private:
    U8X8_SSD1306_128X32_UNIVISION_HW_I2C oled; // Hardware I2C OLED display
    char line1[17] = "................"; // Line 1 buffer (16 chars + null)
    char line2[17] = "................"; // Line 2 buffer (16 chars + null)
    char buffer[17] = {0}; // Shared buffer for string conversions (16 chars + null)
    uint8_t cursorCol; // Current cursor column for line 3

    // Internal method to print text to line 3 and update cursor
    void printInternal(const char *text, bool newLine);

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

    /// Refresh lines 1 and 2 with current status
    void refreshDisplay();
};
