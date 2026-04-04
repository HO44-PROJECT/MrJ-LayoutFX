/**
 * @file DebugOled.h
 * @brief Header for OLED display utilities for debugging output.
 *
 * Provides helper functions to print formatted debug text on an
 * SSD1306 128x32 OLED screen using the U8g2 library in page buffer mode.
 * Supports print, println, and printf from both RAM and PROGMEM (F()) strings,
 * with automatic scrolling of up to 4 lines.
 *
 * @author MrJ
 * @date 2025-09-03
 * @license MIT License
 */

#pragma once

#include "utils/utils.h"

#ifdef DEBUG_OLED

#include <U8g2lib.h>
#include <WString.h>

// OLED object declaration (defined in DebugOled.cpp)
extern U8G2_SSD1306_128X32_UNIVISION_1_SW_I2C oled;

/**
 * @brief Initialize the OLED display.
 * @param speed I2C speed (unused for software I2C, kept for compatibility).
 */
void oled_init(long speed = 0);

/**
 * @brief Redraws the OLED with the contents of the internal buffer.
 */
void oled_draw();

/**
 * @brief Clears the OLED display and resets the buffer.
 */
void oled_clear();

/**
 * @brief Prints a formatted string (RAM) without newline.
 * @param fmt Format string in RAM.
 * @param ... Variable arguments.
 */
void oled_print(const char *fmt, ...);

/**
 * @brief Prints a formatted string (RAM) with newline.
 * @param fmt Format string in RAM.
 * @param ... Variable arguments.
 */
void oled_println(const char *fmt, ...);

/**
 * @brief Prints a formatted string (RAM), alias of oled_print.
 * @param fmt Format string in RAM.
 * @param ... Variable arguments.
 */
void oled_printf(const char *fmt, ...);

/**
 * @brief Prints a formatted string (PROGMEM, use F()) without newline.
 * @param fmt Format string in PROGMEM.
 * @param ... Variable arguments.
 */
void oled_print(const __FlashStringHelper *fmt, ...);

/**
 * @brief Prints a formatted string (PROGMEM, use F()) with newline.
 * @param fmt Format string in PROGMEM.
 * @param ... Variable arguments.
 */
void oled_println(const __FlashStringHelper *fmt, ...);

/**
 * @brief Prints a formatted string (PROGMEM, use F()), alias of oled_print.
 * @param fmt Format string in PROGMEM.
 * @param ... Variable arguments.
 */
void oled_printf(const __FlashStringHelper *fmt, ...);

#endif // DEBUG_OLED
