/**
 * @file StatusOled.cpp
 * @brief Implementation of the `StatusOled` class for managing a 128x32 OLED display.
 *
 * This file implements the `StatusOled` class, providing methods to initialize and control
 * a 128x32 OLED display using the U8g2 library over I2C. It supports printing text to line 3
 * and updating status indicators on lines 1 and 2. A single shared buffer minimizes SRAM usage,
 * and unified print methods reduce code size. Designed for Arduino Nano in the
 * MrJ-ArduinoRailwayFX project.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-09-14
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifdef OLED

#include "oled/StatusOled.h"

/**
 * @brief Constructor for StatusOled
 * @details Initializes the OLED display object with no I2C pins specified and sets the cursor column to 0.
 *          Also initializes the internal buffer to an empty string.
 */
StatusOled::StatusOled() : oled(U8X8_PIN_NONE), cursorCol(0)
{
    // Initialize buffer to empty string
    buffer[0] = '\0';
}

/**
 * @brief Initialize OLED display and I2C
 * @details Initializes the I2C communication and OLED display, sets contrast, font, and clears the display.
 */
void StatusOled::begin()
{
    // Initialize I2C communication
    Wire.begin();
    // Initialize OLED display
    oled.begin();
    // Wait for OLED stabilization
    delay(200);
    // Set display contrast to medium value
    oled.setContrast(128);
    // Set 5x7 font allowing 16 characters per line
    oled.setFont(u8x8_font_chroma48medium8_u);
    // Clear the display buffer
    oled.clearDisplay();
    // Update the display with cleared content
    oled.display();
}

/**
 * @brief Update status on lines 1 and 2 for given ID and value
 * @param id The column position (0-15) to update
 * @param value The character to display at the specified position
 * @details Updates a single character on line 1 or 2 based on the provided ID.
 */
void StatusOled::updateVisibleStatus(uint8_t id, char value)
{
    // Check if ID is within valid range (0-15)
    if (id < 16)
    {
        // Set cursor to specified column on line 1
        oled.setCursor(id, 1);
        // Print the character
        oled.print(value);
        // Update the display
        oled.display();
    }
}

/**
 * @brief Set label on line 0 for given ID and value
 * @param id The column position (0-15) to update
 * @param value The character to display at the specified position
 * @details Updates a single character on line 0 based on the provided ID.
 */
void StatusOled::label(uint8_t id, char value)
{
    // Check if ID is within valid range (0-15)
    if (id < 16)
    {
        // Set cursor to specified column on line 0
        oled.setCursor(id, 0);
        // Print the character
        oled.print(value);
        // Update the display
        oled.display();
    }
}

/**
 * @brief Internal method to print text to line 3 and update cursor
 * @param text The text to print
 * @param newLine Whether to reset the cursor to the start of the line after printing
 * @details Prints text to line 3 at the current cursor position, updates the cursor, and handles line wrapping.
 */
void StatusOled::printInternal(const char *text, bool newLine)
{
    // Skip if text is null or empty
    if (!text || !text[0])
        return;
    // Limit length to display width (16 chars)
    size_t len = strlen(text);
    if (len > 16)
        len = 16;
    // Print to line 3 at current cursor position
    oled.setCursor(cursorCol, 2);
    // Print the text
    oled.print(text);
    // Update the display
    oled.display();
    // Update cursor position based on text length
    cursorCol += len;
    // Reset cursor if newline requested or line is full
    if (newLine || cursorCol >= 16)
    {
        cursorCol = 0;
    }
}

/**
 * @brief Print SRAM string to line 3
 * @param text The SRAM string to print
 * @param newLine Whether to reset the cursor to the start of the line after printing
 * @details Copies the SRAM string to the internal buffer and prints it to line 3.
 */
void StatusOled::print(const char *text, bool newLine)
{
    // Skip if text is null
    if (!text)
        return;
    // Get length of the input string
    size_t len = strlen(text);
    // Limit length to buffer size
    if (len >= sizeof(buffer))
        len = sizeof(buffer) - 1;
    // Copy string to internal buffer
    strncpy(buffer, text, len);
    buffer[len] = '\0';
    // Print the buffer content
    printInternal(buffer, newLine);
}

/**
 * @brief Print flash string (F()) to line 3
 * @param text The flash string (PROGMEM) to print
 * @param newLine Whether to reset the cursor to the start of the line after printing
 * @details Copies the flash string to the internal buffer and prints it to line 3.
 */
void StatusOled::print(const __FlashStringHelper *text, bool newLine)
{
    // Skip if text is null
    if (!text)
        return;
    // Cast to PROGMEM pointer
    PGM_P p = reinterpret_cast<PGM_P>(text);
    // Get length of the flash string
    size_t len = strlen_P(p);
    // Limit length to buffer size
    if (len >= sizeof(buffer))
        len = sizeof(buffer) - 1;
    // Copy flash string to internal buffer
    strncpy_P(buffer, p, len);
    buffer[len] = '\0';
    // Print the buffer content
    printInternal(buffer, newLine);
}

/**
 * @brief Print 32-bit integer to line 3
 * @param value The 32-bit integer to print
 * @param newLine Whether to reset the cursor to the start of the line after printing
 * @details Converts the integer to a string and prints it to line 3.
 */
void StatusOled::print(int value, bool newLine)
{
    // Convert 32-bit integer to string in the buffer
    itoa(value, buffer, 10);
    // Print the buffer content
    printInternal(buffer, newLine);
}

/**
 * @brief Print 8-bit unsigned integer to line 3
 * @param value The 8-bit unsigned integer to print
 * @param newLine Whether to reset the cursor to the start of the line after printing
 * @details Converts the unsigned integer to a string and prints it to line 3.
 */
void StatusOled::print(uint8_t value, bool newLine)
{
    // Convert 8-bit unsigned integer to string in the buffer
    utoa(value, buffer, 10);
    // Print the buffer content
    printInternal(buffer, newLine);
}

/**
 * @brief Refresh lines 1 and 2 with current status
 * @details Redraws the content of lines 1 and 2 using stored status data.
 */
void StatusOled::refreshDisplay()
{
    // Set cursor to start of line 0
    oled.setCursor(0, 0);
    // Print content of line 1
    oled.print(line1);
    // Set cursor to start of line 1
    oled.setCursor(0, 1);
    // Print content of line 2
    oled.print(line2);
    // Update the display
    oled.display();
}

#endif // OLED