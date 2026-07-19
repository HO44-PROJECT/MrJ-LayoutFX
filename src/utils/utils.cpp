/**
 * @file utils.h
 * @brief Utilities for memory allocation and serial debugging.
 *
 * This file contains a collection of helper functions, including memory
 * duplication and serial port initialization, to be used across the project.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license AGPL-3.0-or-later
 */

#include "utils/utils.h"

#ifdef LOG_SERIAL
// Tier-2 serial logging is ON by default (matches the compiled LOG_SERIAL flag);
// the uart0 "log" bus in config may flip it off at boot to free GPIO1/3.
bool g_lfxLogActive = true;
#endif

#if defined(ESP32)
// Bi-core device-list mutex (backlog #27). Created at static init (FreeRTOS is up
// well before the HTTP task starts), so lock/unlock are always safe.
SemaphoreHandle_t g_lfxDeviceMutex = xSemaphoreCreateMutex();
#endif

/// @brief Allocates memory and duplicates the contents of a source buffer.
/// @param in A pointer to the source memory buffer.
/// @param size The size of the memory buffer to duplicate.
/// @return A pointer to the newly allocated and duplicated memory, or NULL if allocation fails.
void *duplicate(const void *in, const size_t size)
{

  void *out = malloc(size);
  if (out != NULL)
  {
    memcpy(out, in, size);
  }
  return out;
}

/** @brief Print SRAM string to Serial */
void debugPrint(const char *text)
{
    if (text) Serial.print(text);
}

/** @brief Print flash string (F() or PROGMEM) to Serial */
void debugPrint(const __FlashStringHelper *text)
{
    if (text) Serial.print(text);
}

/** @brief Print 32-bit integer to Serial */
void debugPrint(int value)
{
    Serial.print(value);
}

/** @brief Print float to Serial with specified decimals */
void debugPrint(float value, uint8_t decimals)
{
    char buf[32];
    dtostrf(value, 0, decimals, buf); // Convert float to string
    Serial.print(buf);
}

/** @brief Print 8-bit unsigned integer (byte) to Serial */
void debugPrint(uint8_t value)
{
    Serial.print(value);
}

/** @brief Print size_t to Serial */
void debugPrint(size_t value)
{
    Serial.print(value);
}

/** @brief Print SRAM string to Serial with newline */
void debugPrintln(const char *text)
{
    if (text) Serial.println(text);
}

/** @brief Print flash string (F() or PROGMEM) to Serial with newline */
void debugPrintln(const __FlashStringHelper *text)
{
    if (text) Serial.println(text);
}

/** @brief Print 32-bit integer to Serial with newline */
void debugPrintln(int value)
{
    Serial.println(value);
}

/** @brief Print float to Serial with newline and specified decimals */
void debugPrintln(float value, uint8_t decimals)
{
    char buf[32];
    dtostrf(value, 0, decimals, buf); // Convert float to string
    Serial.println(buf);
}

/** @brief Print 8-bit unsigned integer (byte) to Serial with newline */
void debugPrintln(uint8_t value)
{
    Serial.println(value);
}

/** @brief Print size_t to Serial with newline */
void debugPrintln(size_t value)
{
    Serial.println(value);
}

static uint16_t lfsr = 0xACE1; // Graine initiale
uint8_t getPseudoRandom(uint8_t min, uint8_t max) {
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400); // LFSR 16 bits
    return min + (lfsr % (max - min));
}