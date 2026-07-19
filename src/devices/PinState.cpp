/**
 * @file PinState.h
 * @brief Defines constants for different pin states and an equality operator.
 *
 * This file provides a simple structure to represent different configurations
 * for an Arduino pin (mode and value) and defines common presets like
 * INPUT, INPUT_PULLUP, and OUTPUT states.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#include "devices/PinState.h"

/**
 * @brief Overloads the equality operator for PIN_STATE structures.
 *
 * This allows two PIN_STATE objects to be compared simply by their unique
 * character ID.
 *
 * @param x The first PIN_STATE object.
 * @param y The second PIN_STATE object.
 * @return true if the IDs are equal, false otherwise.
 */
bool operator==(const PIN_STATE &x, const PIN_STATE &y) { return x.id == y.id; }

// Common predefined PIN_STATE constants.
// These constants are used to easily configure a pin's mode and initial value.

/**
 * @brief Represents an input pin with no internal pull-up.
 *
 * Mode: INPUT
 * Value: LOW (initial state)
 */
const PIN_STATE Z = { .id='Z', .mode = INPUT, .value = LOW };

/**
 * @brief Represents an input pin with the internal pull-up enabled.
 *
 * Mode: INPUT_PULLUP
 * Value: LOW (initial state)
 */
const PIN_STATE P = { .id='P', .mode = INPUT_PULLUP, .value = LOW };

/**
 * @brief Represents an output pin with a LOW (off) initial value.
 *
 * Mode: OUTPUT
 * Value: LOW
 */
const PIN_STATE L = { .id='L', .mode = OUTPUT, .value = LOW };

/**
 * @brief Represents an output pin with a HIGH (on) initial value.
 *
 * Mode: OUTPUT
 * Value: HIGH
 */
const PIN_STATE H = { .id='H', .mode = OUTPUT, .value = HIGH };

const PIN_STATE SH = { .id='h', .mode = OUTPUT, .value = HIGH };

const PIN_STATE I = { .id='I', .mode = PIN_NO_MODE, .value = PIN_NO_VALUE };
