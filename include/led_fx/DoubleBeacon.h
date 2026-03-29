/**
 * @file DoubleBeacon.h
 * @brief Defines the `DoubleBeacon` class for a dual-pin flashing beacon light effect.
 *
 * This class simulates a beacon light with a repeating pattern of two short flashes
 * on alternating pins, followed by a longer pause, suitable for model railway signals
 * or warning lights. It inherits from `AlternateLedEffect` to manage two output pins
 * using a coroutine-based state machine.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __DOUBLEBEACON_H__
#define __DOUBLEBEACON_H__

#include "led_fx/AlternateLedEffect.h"

/**
 * @class DoubleBeacon
 * @brief Simulates a dual-pin flashing beacon light effect.
 *
 * This class extends `AlternateLedEffect` to manage two LED pins using a
 * coroutine-based state machine, implementing a double-flash pattern with
 * precise timing for model railway signals or warning lights.
 */
class DoubleBeacon : public AlternateLedEffect
{
public:
    using AlternateLedEffect::AlternateLedEffect;

    static const STATE_TYPE RUN_FIRST = NEXT_STABLE;  ///< First flash on the first pin.
    static const STATE_TYPE RUN_SECOND = NEXT_STABLE +1; ///< Second flash on the second pin.

    /**
     * @brief Retrieves the device name for identification.
     * @return The C-string "DoubleBeacon".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("DoubleBeacon");
    }

    /**
     * @brief Executes the coroutine for the double beacon flash pattern.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    PIN_ID working_pin = NO_PIN; ///< Current active pin for flashing (first or second pin).

private:
    uint32_t timerStart = 0; ///< Timestamp for tracking delays in the flash pattern (ms).
};

#endif // __DOUBLEBEACON_H__