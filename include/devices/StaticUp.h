/**
 * @file StaticUp.h
 * @brief Defines the `StaticUp` class, a device that keeps inactive pins HIGH.
 *
 * This file provides the class definition for a device that, similar to `StaticLow`,
 * manages a set of pins. However, its key characteristic is that it
 * sets any inactive pins to a HIGH state, rather than a LOW or INPUT state. It is a
 * concrete implementation of the `VariablePinDevice` base class.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#pragma once

#include "VariablePinDevice.h"

/**
 * @class StaticUp
 * @brief Manages a static ON_STATE/OFF_STATE light effect with inactive pins set to HIGH.
 *
 * This class extends `VariablePinDevice` to provide a state machine for
 * controlling multiple pins. It's distinct from other static implementations
 * in that it explicitly sets unused pins to a HIGH output state, which is useful for
 * certain hardware configurations (e.g., driving hardware that requires an active
 * HIGH signal to be off, or for pull-up resistor configurations).
 */
class StaticUp : public VariablePinDevice
{
protected:
    PIN_STATE inactive_state = H; ///< Defines the default state for inactive pins (OUTPUT HIGH).
};
