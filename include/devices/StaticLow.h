/**
 * @file StaticLow.h
 * @brief Defines the `StaticLow` class, a device that keeps inactive pins LOW.
 *
 * This file provides the class definition for a device that, similar to `StaticOpen`,
 * manages a set of pins. However, its key characteristic is that it
 * sets any inactive pins to a LOW state, rather than an INPUT state. It is a
 * concrete implementation of the `VariablePinDevice` base class.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#pragma once

#include "VariablePinDevice.h"

/**
 * @class StaticLow
 * @brief Manages a static ON_STATE/OFF_STATE light effect with inactive pins set to LOW.
 *
 * This class extends `VariablePinDevice` to provide a state machine for
 * controlling multiple pins. It's distinct from `StaticOpen` in that it
 * explicitly sets unused pins to a LOW output state, which is useful for
 * certain hardware configurations (e.g., controlling relays or transistors
 * that require a definite LOW signal to be off).
 */
class StaticLow : public VariablePinDevice
{
public:
    using VariablePinDevice::VariablePinDevice;

    StaticLow(const size_t pin_count, const PIN_ID pins[]) : VariablePinDevice(pin_count, pins) {
        initPins();
        setState(RUN_STABLE_STATE);
    }

    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("StaticLow");
    }

    inline virtual char statusChar() {
        return '_';
    }

    /**
     * @brief Executes the coroutine for asynchronous state transitions.
     *
     * Provides a basic ON_STATE/OFF_STATE implementation. Subclasses must override
     * this for complex behaviors.
     *
     * @return The coroutine state as defined by AceRoutine.
     */
    virtual int runCoroutine() { return 0; };

protected:
    PIN_STATE inactive_state = L; ///< Defines the default state for inactive pins (OUTPUT LOW).
};
