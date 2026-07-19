/**
 * @file StatusOled.cpp
 * @brief Implementation of the `StatusOled` class for managing a 128x32 OLED display.
 *
 * This file implements the `StatusOled` class, providing methods to initialize and control
 * a 128x32 OLED display using the U8g2 library over I2C. It supports printing text to line 3
 * and updating status indicators on lines 1 and 2. A single shared buffer minimizes SRAM usage,
 * and unified print methods reduce code size. Designed for Arduino Nano in the
 * MrJ-LayoutFX project.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-09-14
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "oled/StatusOled.h"

#ifdef OLED_STATUS

// Global singleton — referenced by the STATUS()/STATUS_LABEL() macros in utils.h.
StatusOled oled_status;

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

    // If newLine requested, clear the entire line first
    if (newLine || cursorCol == 0)
    {
        oled.setCursor(0, 3);
        oled.print("                "); // Clear line with 16 spaces
        cursorCol = 0;
    }

    // Limit length to display width (16 chars)
    size_t len = strlen(text);
    if (len > 16)
        len = 16;

    // Print to row 3 (log row) at current cursor position
    oled.setCursor(cursorCol, 3);
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
 * @brief Show a two-line splash screen and clear after delayMs
 * @param name Project name (flash string)
 * @param version Version string (flash string)
 * @param delayMs How long to show the splash (ms, default 1500)
 * @details Displays a centered splash screen with project name and version.
 */
void StatusOled::showSplash(const __FlashStringHelper *name,
                            const __FlashStringHelper *version,
                            uint16_t delayMs)
{
    // Clear display
    oled.clearDisplay();

    // Line 1: project name (centered)
    if (name)
    {
        PGM_P p = reinterpret_cast<PGM_P>(name);
        size_t len = strlen_P(p);
        if (len > 16)
            len = 16;
        strncpy_P(buffer, p, len);
        buffer[len] = '\0';

        // Center text: (16 - len) / 2
        uint8_t offset = (len < 16) ? (16 - len) / 2 : 0;
        oled.setCursor(offset, 1);
        oled.print(buffer);
    }

    // Line 2: version (centered)
    if (version)
    {
        PGM_P p = reinterpret_cast<PGM_P>(version);
        size_t len = strlen_P(p);
        if (len > 16)
            len = 16;
        strncpy_P(buffer, p, len);
        buffer[len] = '\0';

        // Center text: (16 - len) / 2
        uint8_t offset = (len < 16) ? (16 - len) / 2 : 0;
        oled.setCursor(offset, 2);
        oled.print(buffer);
    }

    // Update display
    oled.display();

    // Wait for specified delay
    delay(delayMs);

    // Clear display after delay
    oled.clearDisplay();
    oled.display();
}

/**
 * @brief Write a fixed info message to row 2
 * @param text SRAM string (max 16 chars)
 * @details Overwrites the full row to clear previous content.
 */
void StatusOled::info(const char *text)
{
    // Set cursor to start of line 2
    oled.setCursor(0, 2);
    // Print text (truncated to 16 chars)
    if (text)
    {
        size_t len = strlen(text);
        if (len > 16)
            len = 16;
        // Print the text
        for (size_t i = 0; i < 16; i++)
        {
            if (i < len)
                oled.print(text[i]);
            else
                oled.print(' '); // Pad with spaces
        }
    }
    else
    {
        // Clear line if text is null
        oled.print("                ");
    }
    // Update display
    oled.display();
}

/**
 * @brief info() overload accepting F() strings
 * @param text Flash string (max 16 chars)
 * @details Overwrites the full row to clear previous content.
 */
void StatusOled::info(const __FlashStringHelper *text)
{
    // Set cursor to start of line 2
    oled.setCursor(0, 2);
    // Print text (truncated to 16 chars)
    if (text)
    {
        PGM_P p = reinterpret_cast<PGM_P>(text);
        size_t len = strlen_P(p);
        if (len >= sizeof(buffer))
            len = sizeof(buffer) - 1;
        strncpy_P(buffer, p, len);
        buffer[len] = '\0';
        // Print with padding
        for (size_t i = 0; i < 16; i++)
        {
            if (i < len)
                oled.print(buffer[i]);
            else
                oled.print(' '); // Pad with spaces
        }
    }
    else
    {
        // Clear line if text is null
        oled.print("                ");
    }
    // Update display
    oled.display();
}

/**
 * @brief Clear a specific line (0-3)
 * @param line Line number to clear (0-3)
 * @details Fills the specified line with 16 spaces to clear previous content.
 */
void StatusOled::clearLine(uint8_t line)
{
    // Check if line is within valid range (0-3)
    if (line < 4)
    {
        // Set cursor to start of line
        oled.setCursor(0, line);
        // Print 16 spaces to clear the line
        oled.print("                ");
        // Update the display
        oled.display();
    }
}

/**
 * @brief Print text at a specific position without affecting cursor
 * @param col Column position (0-15)
 * @param row Row position (0-3)
 * @param text Text to print (SRAM string)
 * @details Prints text at the specified position without modifying cursorCol.
 */
void StatusOled::printAt(uint8_t col, uint8_t row, const char *text)
{
    // Check if position is within valid range
    if (row < 4 && col < 16 && text)
    {
        // Set cursor to specified position
        oled.setCursor(col, row);
        // Print the text
        oled.print(text);
        // Update the display
        oled.display();
    }
}

/**
 * @brief Print flash string at a specific position
 * @param col Column position (0-15)
 * @param row Row position (0-3)
 * @param text Flash string (PROGMEM) to print
 * @details Copies flash string to buffer and prints at specified position.
 */
void StatusOled::printAt(uint8_t col, uint8_t row, const __FlashStringHelper *text)
{
    // Check if position is within valid range
    if (row < 4 && col < 16 && text)
    {
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
        // Set cursor to specified position
        oled.setCursor(col, row);
        // Print the buffer content
        oled.print(buffer);
        // Update the display
        oled.display();
    }
}

/**
 * @brief Set display contrast level
 * @param level Contrast level (0-255)
 * @details Adjusts the OLED display brightness/contrast.
 */
void StatusOled::setContrast(uint8_t level)
{
    oled.setContrast(level);
}

/**
 * @brief Draw a progress bar on the specified row
 * @param row Row position (0-3)
 * @param value Current value
 * @param max Maximum value
 * @details Draws a 16-character progress bar [##########.....] format.
 */
void StatusOled::drawBar(uint8_t row, uint8_t value, uint8_t max)
{
    // Check if row is within valid range
    if (row >= 4 || max == 0)
        return;

    // Calculate filled portion (14 chars for content, 2 for brackets)
    uint8_t filled = (value > max) ? 14 : ((uint32_t)value * 14) / max;

    // Set cursor to start of row
    oled.setCursor(0, row);
    // Print opening bracket
    oled.print('[');
    // Print filled and empty portions
    for (uint8_t i = 0; i < 14; i++)
    {
        oled.print(i < filled ? '#' : '.');
    }
    // Print closing bracket
    oled.print(']');
    // Update the display
    oled.display();
}

/**
 * @brief Enable or disable inverse video mode
 * @param enabled True to invert colors (black on white)
 * @details Inverts all pixels on the display.
 */
void StatusOled::inverse(bool enabled)
{
    oled.setInverseFont(enabled ? 1 : 0);
}

/**
 * @brief Enable or disable power save mode
 * @param enabled True to turn off display, false to turn on
 * @details Puts the display in low-power mode without losing content.
 */
void StatusOled::powerSave(bool enabled)
{
    oled.setPowerSave(enabled ? 1 : 0);
}

/**
 * @brief Flip display orientation 180 degrees
 * @param flip True to flip, false for normal orientation
 * @details Useful when the OLED is mounted upside down.
 */
void StatusOled::setFlipMode(bool flip)
{
    oled.setFlipMode(flip ? 1 : 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// High-level helpers
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Show device event details temporarily
 * @param deviceId Device identifier (SRAM string)
 * @param state State name or value
 * @param value Optional numeric value (e.g., PWM level, 0-255)
 * @details Displays device event on all 4 lines:
 *   Line 0: device ID
 *   Line 1: state
 *   Line 2: value (if non-zero)
 *   Line 3: progress bar (if value > 0)
 */
void StatusOled::showEvent(const char *deviceId, const char *state, uint8_t value)
{
    // Clear display
    oled.clearDisplay();

    // Line 0: device ID
    oled.setCursor(0, 0);
    if (deviceId)
        oled.print(deviceId);

    // Line 1: state
    oled.setCursor(0, 1);
    if (state)
        oled.print(state);

    // Line 2: value (if non-zero)
    if (value > 0)
    {
        oled.setCursor(0, 2);
        oled.print("Value: ");
        utoa(value, buffer, 10);
        oled.print(buffer);
    }

    // Line 3: progress bar (if value > 0)
    if (value > 0)
    {
        drawBar(3, value, 255);
    }

    // Update display
    oled.display();
}

/**
 * @brief Show device event details (F() string overload)
 */
void StatusOled::showEvent(const __FlashStringHelper *deviceId, const __FlashStringHelper *state, uint8_t value)
{
    // Clear display
    oled.clearDisplay();

    // Line 0: device ID
    if (deviceId)
    {
        PGM_P p = reinterpret_cast<PGM_P>(deviceId);
        size_t len = strlen_P(p);
        if (len >= sizeof(buffer))
            len = sizeof(buffer) - 1;
        strncpy_P(buffer, p, len);
        buffer[len] = '\0';
        oled.setCursor(0, 0);
        oled.print(buffer);
    }

    // Line 1: state
    if (state)
    {
        PGM_P p = reinterpret_cast<PGM_P>(state);
        size_t len = strlen_P(p);
        if (len >= sizeof(buffer))
            len = sizeof(buffer) - 1;
        strncpy_P(buffer, p, len);
        buffer[len] = '\0';
        oled.setCursor(0, 1);
        oled.print(buffer);
    }

    // Line 2: value (if non-zero)
    if (value > 0)
    {
        oled.setCursor(0, 2);
        oled.print("Value: ");
        utoa(value, buffer, 10);
        oled.print(buffer);
    }

    // Line 3: progress bar (if value > 0)
    if (value > 0)
    {
        drawBar(3, value, 255);
    }

    // Update display
    oled.display();
}

/**
 * @brief Display system metrics (RAM, uptime, device count)
 * @param freeRam Free RAM in bytes
 * @param deviceCount Number of active devices (0 to skip)
 * @details Shows on line 2:
 *   "RAM:1234 D:05" or "RAM:1234" if deviceCount == 0
 */
void StatusOled::showMetrics(uint16_t freeRam, uint8_t deviceCount)
{
    // Build complete line in buffer to avoid display artifacts
    char line[17];
    if (deviceCount > 0)
    {
        snprintf(line, sizeof(line), "RAM:%-4u D:%02u   ", freeRam, deviceCount);
    }
    else
    {
        snprintf(line, sizeof(line), "RAM:%-4u        ", freeRam);
    }

    // Line 2: display complete line
    oled.setCursor(0, 2);
    oled.print(line);

    // Update display
    oled.display();
}

/**
 * @brief Flash an error message with inverse video
 * @param message Error message (SRAM string)
 * @param flashState Toggle this boolean each call to create flash effect
 * @details Overwrites entire display with error message.
 *   Call repeatedly in loop() to maintain flash effect.
 */
void StatusOled::flashError(const char *message, bool flashState)
{
    // Set inverse mode based on flash state
    inverse(flashState);

    // Clear display
    oled.clearDisplay();

    // Line 0: ERROR header
    oled.setCursor(0, 0);
    oled.print("!!! ERROR !!!");

    // Line 1-2: message (word wrap if needed)
    if (message)
    {
        size_t len = strlen(message);
        oled.setCursor(0, 1);
        if (len <= 16)
        {
            // Single line
            oled.print(message);
        }
        else
        {
            // Split across two lines
            char line1[17] = {0};
            char line2[17] = {0};
            strncpy(line1, message, 16);
            strncpy(line2, message + 16, 16);
            oled.print(line1);
            oled.setCursor(0, 2);
            oled.print(line2);
        }
    }

    // Line 3: instruction
    oled.setCursor(0, 3);
    oled.print("Press reset");

    // Update display
    oled.display();
}

/**
 * @brief Flash an error message (F() string overload)
 */
void StatusOled::flashError(const __FlashStringHelper *message, bool flashState)
{
    // Set inverse mode based on flash state
    inverse(flashState);

    // Clear display
    oled.clearDisplay();

    // Line 0: ERROR header
    oled.setCursor(0, 0);
    oled.print("!!! ERROR !!!");

    // Line 1-2: message
    if (message)
    {
        PGM_P p = reinterpret_cast<PGM_P>(message);
        size_t len = strlen_P(p);

        oled.setCursor(0, 1);
        if (len <= 16)
        {
            // Single line
            strncpy_P(buffer, p, 11);
            buffer[11] = '\0';
            oled.print(buffer);
        }
        else
        {
            // Split across two lines
            char line1[17] = {0};
            char line2[17] = {0};
            strncpy_P(line1, p, 16);
            strncpy_P(line2, p + 16, 16);
            oled.print(line1);
            oled.setCursor(0, 2);
            oled.print(line2);
        }
    }

    // Line 3: instruction
    oled.setCursor(0, 3);
    oled.print("Press reset");

    // Update display
    oled.display();
}

/**
 * @brief Get free RAM (ATmega only)
 * @return Free RAM in bytes
 * @details Calculates free RAM by measuring stack/heap gap.
 *   Only works on AVR (Arduino Nano, Uno, Mega).
 */
uint16_t StatusOled::getFreeRam()
{
#ifdef __AVR__
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
#else
    return 0; // Not AVR, return 0
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Auto-update system
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Register a device for auto-tracking
 * @param id Device ID (0-15)
 * @param label Character label to display on line 0
 */
void StatusOled::registerDevice(uint8_t id, char label)
{
    if (id < 16)
    {
        this->label(id, label);
        if (id >= deviceCount)
        {
            deviceCount = id + 1;
        }
    }
}

/**
 * @brief Update device state and refresh display automatically
 * @param id Device ID (0-15)
 * @param state New state (0=OFF, non-zero=ON)
 * @param showDetailedEvent Show full event screen if state changed
 */
void StatusOled::updateDevice(uint8_t id, uint8_t state, bool showDetailedEvent)
{
    if (id >= 16)
        return;

    // Check if state changed
    bool changed = (deviceStates[id] != state);
    deviceStates[id] = state;

    if (!changed)
        return;

    // Update status line
    char symbol = state ? '#' : '.';
    updateVisibleStatus(id, symbol);

    // Log the change
    char msg[17];
    snprintf(msg, sizeof(msg), "D%u: %s", id, state ? "ON" : "OFF");
    print(msg, true);

    // Show detailed event if requested
#ifdef OLED_DEBUG_EVENTS
    showDetailedEvent = true;
#endif

    if (showDetailedEvent)
    {
        char devId[6];
        snprintf(devId, sizeof(devId), "D%u", id);
        showEvent(devId, state ? "ON" : "OFF", state ? 255 : 0);
        inEventMode = true;
        eventDisplayEnd = millis() + 3000; // 3 seconds
    }
}

/**
 * @brief Main loop update
 * @details Call this in loop() to auto-refresh display.
 */
void StatusOled::loop()
{
    unsigned long now = millis();

    // Handle event display timeout
    if (inEventMode && now >= eventDisplayEnd)
    {
        inEventMode = false;
        restoreNormalDisplay();
        print(F("Back to normal"), true);
    }

    // Update metrics periodically (every 5 seconds)
#ifdef OLED_DEBUG_METRICS
    if (!inEventMode && (now - lastMetricsUpdate >= 5000))
    {
        showMetrics(getFreeRam(), deviceCount);
        lastMetricsUpdate = now;
    }
#endif
}

/**
 * @brief Restore normal display after event
 */
void StatusOled::restoreNormalDisplay()
{
    // Clear all lines
    for (uint8_t i = 0; i < 4; i++)
    {
        clearLine(i);
    }

    // Restore labels (line 0) and states (line 1)
    for (uint8_t i = 0; i < deviceCount; i++)
    {
        // Labels restored via registerDevice() call in setup
        char symbol = deviceStates[i] ? '#' : '.';
        updateVisibleStatus(i, symbol);
    }

    // Restore metrics
#ifdef OLED_DEBUG_METRICS
    showMetrics(getFreeRam(), deviceCount);
#endif
}

#endif // OLED_STATUS