/**
 * @file PinStateGPIO.h
 * @brief PIN_ID as a plain uint8_t — for GPIO-only platforms (Arduino Nano, Uno, Mega…).
 *
 * Do not include directly. Include devices/PinState.h which routes to this file
 * or to PinStateSPI.h depending on the LFX_SPI_CARDS_ENABLED build flag.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @license AGPL-3.0-or-later
 */

#pragma once

#include <Arduino.h>
#include "utils/ArduinoBoard.h"

#define NO_PIN       255          ///< Unassigned or invalid pin.
#define PIN_NO_MODE  255
#define PIN_NO_VALUE 255
#define VALID_PIN(p) ((p) < MAX_PIN_NUMBER && (p) != NO_PIN)

typedef uint8_t PIN_ID;           ///< Pin identifier — GPIO pin number (0–MAX_PIN_NUMBER-1).

/** @brief Return a uint8_t OLED display ID from a PIN_ID (GPIO mode: identity). */
inline uint8_t pinId(PIN_ID p) { return p; }

/** @brief Write HIGH or LOW to a PIN_ID (GPIO mode: direct digitalWrite). */
inline void pinWrite(PIN_ID p, uint8_t value) { digitalWrite(p, value); }

/**
 * @struct PIN_STATE
 * @brief Mode and initial value of a pin (INPUT/OUTPUT + LOW/HIGH).
 */
typedef struct {
  char    id;       ///< Unique character tag (e.g. 'L', 'H', 'Z', 'P').
  uint8_t mode;     ///< Pin mode  (INPUT, OUTPUT, INPUT_PULLUP).
  uint8_t value;    ///< Pin value (LOW, HIGH).
} PIN_STATE;

bool operator==(const PIN_STATE &x, const PIN_STATE &y);

extern const PIN_STATE Z;   ///< INPUT, no pull-up.
extern const PIN_STATE P;   ///< INPUT_PULLUP.
extern const PIN_STATE L;   ///< OUTPUT LOW.
extern const PIN_STATE H;   ///< OUTPUT HIGH.
extern const PIN_STATE I;   ///< Ignore (no mode, no value).
extern const PIN_STATE SH;  ///< Stay HIGH.
