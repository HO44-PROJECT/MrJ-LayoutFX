/**
 * @file PinState.h
 * @brief Defines constants for different pin states and an equality operator.
 *
 * This file provides a simple structure to represent different configurations
 * for an Arduino pin (mode and value) and defines common presets like
 * INPUT, INPUT_PULLUP, and OUTPUT states.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#ifndef __PIN_STATE_H__
#define __PIN_STATE_H__

#include <Arduino.h>
#include "utils/ArduinoBoard.h"

#define NO_PIN 255          ///< A special value to represent an unassigned or invalid pin.
#define PIN_NO_MODE 255
#define PIN_NO_VALUE 255
#define VALID_PIN(p) ((p) < MAX_PIN_NUMBER && (p) != NO_PIN)
typedef uint8_t PIN_ID;     ///< A type definition for pin identifiers.

/**
 * @struct PIN_STATE
 * @brief Defines the mode and value of a pin.
 *
 * This structure is used to easily define and manage the state of a pin
 * (e.g., INPUT, OUTPUT, LOW, HIGH).
 */
typedef struct {
  char id;      ///< A unique character ID for the state (e.g., 'Z', 'P', 'L', 'H').
  uint8_t mode; ///< The pin mode (e.g., INPUT, OUTPUT).
  uint8_t value;///< The digital value (e.g., LOW, HIGH).
} PIN_STATE;

/**
 * @brief Overloads the equality operator for PIN_STATE structures.
 * @param x The first PIN_STATE object.
 * @param y The second PIN_STATE object.
 * @return true if the IDs are equal, false otherwise.
 */
bool operator==(const PIN_STATE &x, const PIN_STATE &y);

// External declarations for common predefined PIN_STATE constants.
extern const PIN_STATE Z; ///< INPUT with no pull-up.
extern const PIN_STATE P; ///< INPUT with internal pull-up.
extern const PIN_STATE L; ///< OUTPUT with LOW value.
extern const PIN_STATE H; ///< OUTPUT with HIGH value.
extern const PIN_STATE I; ///< ignore
extern const PIN_STATE SH; //< Stay HIGH

#endif