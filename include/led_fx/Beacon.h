/**
 * @file Beacon.h
 * @brief Defines the `Beacon` class for a flashing beacon light effect.
 *
 * This header file defines a class that simulates a beacon light with a repeating
 * pattern of two short flashes followed by a longer pause. It inherits from
 * `LedEffect` to manage a single output pin and uses a coroutine for
 * precise timing of the flash pattern.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

/**
 * @class Beacon
 * @brief Simulates a beacon light with a double-flash pattern.
 */
class Beacon : public LedEffect
{
public:
    using LedEffect::LedEffect;

    /**
     * @brief Returns the device name.
     * @return Constant string "Beacon".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Beacon");
    }

    /**
     * @brief Executes the coroutine for the beacon’s double-flash pattern.
     * @return Coroutine state from AceRoutine.
     */
    virtual int runCoroutine() override;

private:
};
