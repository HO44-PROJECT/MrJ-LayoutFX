/**
 * @file Pov.h
 * @brief Macros for implementing Persistence of Vision (POV) effects.
 *
 * This file defines a set of macros to simplify the creation of software-based
 * PWM and delayed execution, specifically for use within an AceRoutine
 * coroutine environment. These macros are designed to provide a non-blocking
 * "lamp" effect on LEDs by rapidly switching them on and off.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#ifndef __POV_H__
#define __POV_H__

#include <AceRoutine.h>
using namespace ace_routine;

/**
 * @brief Implements a raw software PWM cycle with an ON_STATE and an OFF_STATE phase.
 *
 * This macro directly implements a full PWM cycle using `digitalWrite` and
 * `COROUTINE_DELAY_MICROS` for a specified intensity and period. It is useful for
 * creating a constant PWM signal without conditional checks. The use of
 * `do { ... } while (0)` ensures the macro behaves like a single statement.
 *
 * @param pin The Arduino pin number to control.
 * @param intensity The brightness level (0-255).
 * @param period The total duration of one PWM cycle in microseconds.
 */
#define simulatePWM_raw(pin, intensity, period)                             \
  do                                                                        \
  {                                                                         \
    digitalWrite(pin, HIGH);                                                \
    COROUTINE_DELAY_MICROS((uint32_t)(intensity) * (period) / 255);         \
    digitalWrite(pin, LOW);                                                 \
    COROUTINE_DELAY_MICROS((uint32_t)(255 - (intensity)) * (period) / 255); \
  } while (0)

/**
 * @brief Implements a software PWM cycle with checks to avoid unnecessary delays.
 *
 * This macro implements a software PWM cycle similar to `simulatePWM_raw` but
 * includes conditional checks. If the intensity is 0, it only turns the pin OFF_STATE.
 * If the intensity is 255, it only turns the pin ON_STATE, avoiding a zero-length delay.
 * This can be more efficient for extreme intensity values.
 *
 * @param pin The Arduino pin number to control.
 * @param intensity The brightness level (0-255).
 * @param period The total duration of one PWM cycle in microseconds.
 */
#define simulatePWM(pin, intensity, period)                                   \
  do                                                                          \
  {                                                                           \
    if (intensity > 0)                                                        \
    {                                                                         \
      digitalWrite(pin, HIGH);                                                \
      COROUTINE_DELAY_MICROS((uint32_t)(intensity) * (period) / 255);         \
    }                                                                         \
    if (intensity < 255)                                                      \
    {                                                                         \
      digitalWrite(pin, LOW);                                                 \
      COROUTINE_DELAY_MICROS((uint32_t)(255 - (intensity)) * (period) / 255); \
    }                                                                         \
  } while (0)

/**
 * @brief A non-blocking delay macro for use within coroutines.
 *
 * This macro provides a non-blocking delay in milliseconds. It stores the start time
 * and yields control to other coroutines until the specified delay has passed.
 *
 * @param timerStart A variable to store the starting timestamp (must be a `uint32_t` or similar).
 * @param delay_millis The duration of the delay in milliseconds.
 */
#define COROUTINE_DELAY_MILLIS(timerStart, delay_millis)    \
  do                                                        \
  {                                                         \
    timerStart = millis();                                  \
    COROUTINE_AWAIT(millis() - timerStart >= delay_millis); \
  } while (0)

/**
 * @brief Software PWM cycle with inverted phase order (OFF first, then ON).
 *
 * Similar to `simulatePWM` but executes the OFF phase before the ON phase.
 * Useful when the LED must start in the OFF state at the beginning of each cycle.
 *
 * @param pin The Arduino pin number to control.
 * @param intensity The brightness level (0-255).
 * @param period The total duration of one PWM cycle in microseconds.
 */
#define simulatePWM2(pin, intensity, period)                                  \
  do                                                                          \
  {                                                                           \
    if (intensity < 255)                                                      \
    {                                                                         \
      digitalWrite(pin, LOW);                                                 \
      COROUTINE_DELAY_MICROS((uint32_t)(255 - (intensity)) * (period) / 255); \
    }                                                                         \
    if (intensity > 0)                                                        \
    {                                                                         \
      digitalWrite(pin, HIGH);                                                \
      COROUTINE_DELAY_MICROS((uint32_t)(intensity) * (period) / 255);         \
    }                                                                         \
  } while (0)

#endif // __POV_H__
