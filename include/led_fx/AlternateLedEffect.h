

/**
 * @file AlternateLedEffect.h
 * @brief Defines the `AlternateLedEffect` class for alternating LED effects on two pins.
 *
 * This header file defines the `AlternateLedEffect` class, which inherits from
 * `MultiplePinDevice<2>` to manage two output pins for alternating LED effects
 * (e.g., dual-beacon or railway crossing lights). It uses a coroutine-based state
 * machine for non-blocking operation.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-04
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include "devices/MultiplePinDevice.h"
#include "devices/Pov.h"

/**
 * @def ALTERNATE_LED_EFFECT_PIN_COUNT
 * @brief Number of output pins used by an `AlternateLedEffect` instance.
 *
 * Defines the constant number of pins managed by the `AlternateLedEffect` class (always 2).
 */
#define ALTERNATE_LED_EFFECT_PIN_COUNT 2

/**
 * @class AlternateLedEffect
 * @brief Base class for alternating LED effects on two pins.
 *
 * This class extends `MultiplePinDevice<2>` to provide a foundation for effects that
 * alternate between two output pins (e.g., dual beacons, railway crossing lights).
 * It supports asynchronous state transitions using the AceRoutine library.
 */
class AlternateLedEffect : public MultiplePinDevice<ALTERNATE_LED_EFFECT_PIN_COUNT>
{
public:
    using MultiplePinDevice<ALTERNATE_LED_EFFECT_PIN_COUNT>::MultiplePinDevice;

    /**
     * @brief Constant for the device's on state.
     */
    static const STATE_TYPE ON_STATE = 1;

    static const STATE_TYPE NEXT_STABLE = ON_STATE + 1;

    /**
     * @brief Constructs an `AlternateLedEffect` instance with two output pins.
     *
     * Initializes the device with the specified pins. The caller must call `initPins`
     * explicitly to configure the hardware pins.
     *
     * @param pin1 First output pin identifier (default: NO_PIN).
     * @param pin2 Second output pin identifier (default: NO_PIN).
     */
    AlternateLedEffect(PIN_ID pin1 = NO_PIN, PIN_ID pin2 = NO_PIN)
    {
        setPin(0, pin1);
        setPin(1, pin2);
        validatePins();
    }

    /**
     * @brief Activates the alternating LED effect.
     *
     * Requests a transition to ON_STATE to start the alternating effect.
     */
    virtual void switchOn(bool skipDelay = false)
    {
        newState(ON_STATE, skipDelay);
    }

protected:
    PIN_ID working_pin;
};
