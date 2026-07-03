/**
 * @file utils.h
 * @brief Utilities for memory allocation and serial debugging.
 *
 * This file provides a collection of helper functions and macros, including
 * memory duplication and serial port initialization, to be used across the
 * project. It also defines conditional debugging macros.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#pragma once

#include <Arduino.h>
#include <LayoutFX_define.h>

#ifdef DEBUG_OLED
  #include "DebugOled.h"
#endif

#ifdef OLED_STATUS
  #include "oled/StatusOled.h"
#endif

// --- Bi-core device-list lock (backlog #27) ------------------------------------
// One mutex serialises the HTTP handlers (Core 0 system task) against the config
// hot-reload that deletes every Device (Core 1). ESP32-only; a no-op elsewhere
// (AVR is single-core with no HTTP server). Use LFX_DEVICE_LOCK() at the top of
// a scope for RAII lock/unlock.
#if defined(ESP32)
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
extern SemaphoreHandle_t g_lfxDeviceMutex;
struct LfxDeviceLock {
  LfxDeviceLock() { if (g_lfxDeviceMutex) xSemaphoreTake(g_lfxDeviceMutex, portMAX_DELAY); }
  ~LfxDeviceLock() { if (g_lfxDeviceMutex) xSemaphoreGive(g_lfxDeviceMutex); }
};
  #define LFX_DEVICE_LOCK() LfxDeviceLock _lfxDevLock
#else
  #define LFX_DEVICE_LOCK() ((void)0)
#endif

/**
 * @brief Allocates memory and duplicates the contents of a source buffer.
 * @param in A pointer to the source memory buffer.
 * @param size The size of the memory buffer to duplicate.
 * @return A pointer to the newly allocated and duplicated memory, or NULL if allocation fails.
 */
void *duplicate(const void *in, const size_t size);

/**
 * @brief Initializes the serial port for debugging.
 * @param speed The baud rate to use for serial communication.
 */
void init_serial_debug(unsigned long speed);

// --- Conditional Debugging Macros ---

/**
 * @def DEBUG
 * @brief Enables or disables debugging output.
 *
 * Comment out this line to disable all debugging messages.
 */
// #define DEBUG

/** @brief Print SRAM string to Serial */
void debugPrint(const char *text);

/** @brief Print flash string (F() or PROGMEM) to Serial */
void debugPrint(const __FlashStringHelper *text);

/** @brief Print 32-bit integer to Serial */
void debugPrint(int value);

/** @brief Print float to Serial with specified decimals */
void debugPrint(float value, uint8_t decimals = 2);

/** @brief Print 8-bit unsigned integer (byte) to Serial */
void debugPrint(uint8_t value);

/** @brief Print size_t to Serial */
void debugPrint(size_t value);

/** @brief Print SRAM string to Serial with newline */
void debugPrintln(const char *text);

/** @brief Print flash string (F() or PROGMEM) to Serial with newline */
void debugPrintln(const __FlashStringHelper *text);

/** @brief Print 32-bit integer to Serial with newline */
void debugPrintln(int value);

/** @brief Print float to Serial with newline and specified decimals */
void debugPrintln(float value, uint8_t decimals = 2);

/** @brief Print 8-bit unsigned integer (byte) to Serial with newline */
void debugPrintln(uint8_t value);

/** @brief Print size_t to Serial with newline */
void debugPrintln(size_t value);

#ifdef DEBUG_SERIAL

  #define DEBUG_INIT(speed) \
    do {                    \
      Serial.begin(speed);  \
      while (!Serial) {     \
        ;                   \
      }                     \
      delay(1000);          \
    } while (0)

  #define DEBUG_PRINT(x, ...) debugPrint(x, ##__VA_ARGS__)
  #define DEBUG_PRINTLN(x, ...) debugPrintln(x, ##__VA_ARGS__)

#elif defined(DEBUG_OLED)

  #define DEBUG_INIT(speed) oled_init(speed)
  #define DEBUG_PRINT(x, ...) oled_print(x, ##__VA_ARGS__)
  #define DEBUG_PRINTLN(x, ...) oled_println(x, ##__VA_ARGS__)
  #define DEBUG_PRINTF(x, ...) oled_printf(x, ##__VA_ARGS__)

#else

  #define DEBUG_INIT(speed)
  #define DEBUG_PRINT(x, ...)
  #define DEBUG_PRINTLN(x, ...)
  #define DEBUG_PRINTF(x, ...)

#endif

// ---------------------------------------------------------------------------
// Operational logging — LOG_SERIAL and/or LOG_OLED, combinable
// LOG_PRINT  : partial line fragment (Serial only)
// LOG_PRINTLN: complete line (Serial + OLED if LOG_OLED)
// ---------------------------------------------------------------------------

#ifdef LOG_SERIAL
  // Runtime gate for Tier-2 (operational/API) serial logging. Boot-critical
  // Tier-1 messages bypass this and print directly to Serial. Set false — via the
  // uart0 "log" bus in config — to silence Tier-2 and free GPIO1/3 as plain GPIO.
  extern bool g_lfxLogActive;
  #define _LOG_S_PRINT(x) do { if (g_lfxLogActive) Serial.print(x); } while (0)
  #define _LOG_S_PRINTLN(x) do { if (g_lfxLogActive) Serial.println(x); } while (0)
#else
  #define _LOG_S_PRINT(x)
  #define _LOG_S_PRINTLN(x)
#endif

// True when GPIO1/3 are owned by the UART0 console and must not be driven as
// plain GPIO. False once the uart0 log bus is disabled (pins freed for effects),
// so hardware tests and effects can use them. Compile-time false when no serial.
inline bool lfxUart0Reserved() {
#if defined(LOG_SERIAL)
  return g_lfxLogActive;
#elif defined(DEBUG_SERIAL)
  return true;
#else
  return false;
#endif
}

#if defined(LOG_OLED) && defined(LFX_OLED_ENABLED)
  #include "oled/OledDisplay.h"
  inline void _oledLog() {}
  inline void _oledLog(const char *s) { OledDisplay::log(s); }
  inline void _oledLog(const __FlashStringHelper *s) { OledDisplay::log(s); }
  template<typename T> inline void _oledLog(T v) { OledDisplay::log(String(v).c_str()); }
  #define _LOG_O_PRINTLN(...) _oledLog(__VA_ARGS__)
#else
  #define _LOG_O_PRINTLN(...)
#endif

#define LOG_PRINT(x) \
  do {               \
    _LOG_S_PRINT(x); \
  } while (0)
#define LOG_PRINTLN(...) \
  do {                   \
    _LOG_S_PRINTLN(__VA_ARGS__); \
    _LOG_O_PRINTLN(__VA_ARGS__); \
  } while (0)
#ifdef LOG_SERIAL
  #define LOG_PRINTF(fmt, ...) do { if (g_lfxLogActive) Serial.printf(fmt, ##__VA_ARGS__); } while (0)
#else
  #define LOG_PRINTF(fmt, ...)
#endif

#ifdef OLED_STATUS

extern StatusOled oled_status;

  #define STATUS_BEGIN() oled_status.begin()
  #define STATUS_INIT() _statusOledInit()
  #define STATUS_LOOP() oled_status.loop()
  #define STATUS_PRINT(msg) oled_status.print(msg)
  #define STATUS_PRINTLN(msg) oled_status.print(msg, true)
  #define STATUS(id, status) oled_status.updateVisibleStatus(id, status)
  #define STATUS_LABEL(id, value) oled_status.label(id, value)
  #define STATUS_REGISTER_DEVICE(id, label) oled_status.registerDevice(id, label)
  #define STATUS_UPDATE_DEVICE(id, state) oled_status.updateDevice(id, state)

  // Internal init helper (called by LayoutFX::init())
  inline void _statusOledInit() {
    oled_status.begin();
    #ifdef OLED_CONTRAST
      oled_status.setContrast(OLED_CONTRAST);
    #endif
    #ifdef OLED_FLIP_MODE
      oled_status.setFlipMode(true);
    #endif
    oled_status.showSplash(F(LFX_PROJECT_NAME), F(LFX_FIRMWARE_VERSION));
    delay(2000);
  }

#else

  #define STATUS_BEGIN()
  #define STATUS_INIT()
  #define STATUS_LOOP()
  #define STATUS_PRINT(msg)
  #define STATUS_PRINTLN(msg)
  #define STATUS(id, status)
  #define STATUS_LABEL(id, value)
  #define STATUS_REGISTER_DEVICE(id, label)
  #define STATUS_UPDATE_DEVICE(id, state)

#endif

/**
 * @brief Helper function to format PROGMEM strings with %S for Serial output.
 * @param buf Output buffer for the formatted string.
 * @param bufsize Size of the output buffer.
 * @param fmt Format string in PROGMEM.
 * @param ... Variable arguments for formatting.
 */
#ifdef __cplusplus
extern "C" {
#endif
void format_P(char *buf, size_t bufsize, PGM_P fmt, ...);
#ifdef __cplusplus
}
#endif

uint8_t getPseudoRandom(uint8_t min, uint8_t max);
