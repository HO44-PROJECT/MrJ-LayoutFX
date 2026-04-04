/**
 * @file StaticOpen.h
 * @brief Defines the `StaticOpen` class for a simple, non-fading light.
 *
 * This file provides the class definition for a device that simply switches
 * a pin ON_STATE or OFF_STATE without any fading or complex effects. It is a concrete
 * implementation of the `VariablePinDevice` base class.
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
 * @class StaticOpen
 * @brief Manages a static ON_STATE/OFF_STATE light effect with inactive pins as inputs.
 *
 * This class extends `VariablePinDevice` to provide a state machine for
 * controlling a single or multiple pins in a simple, non-fading manner.
 * The key characteristic is that inactive pins are set to the `INPUT` (`Z`)
 * state, which is useful for certain hardware configurations where an output
 * LOW state is not desired.
 */
class StaticOpen : public VariablePinDevice
{
protected:
    /**
     * @brief Defines the default state for inactive pins.
     *
     * In this implementation, inactive pins are set to the `Z` state (INPUT).
     */
    PIN_STATE inactive_state = Z;
};
