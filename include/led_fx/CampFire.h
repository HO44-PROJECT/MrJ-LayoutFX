/**
 * @file CampFire.h
 * @brief Defines the `CampFire` class for a flickering campfire light effect.
 *
 * This header file defines a class that simulates a flickering campfire flame
 * using random intensity fluctuations. It inherits from `LedEffect` to manage a
 * single output pin and uses a coroutine for non-blocking operation.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

/**
 * @class CampFire
 * @brief Simulates a flickering campfire light effect with random intensity changes.
 */
class CampFire : public LedEffect
{
public:
    using LedEffect::LedEffect;

    /**
     * @brief Returns the device name.
     * @return Constant string "CampFire".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("CampFire");
    }

    /**
     * @brief Executes the coroutine for the campfire flicker effect.
     * @return Coroutine state from AceRoutine.
     */
    virtual int runCoroutine() override;

protected:
    /// @brief Current intensity of the campfire flicker (0-255, 8-bit PWM).
    int16_t intensity = CAMPFIRE_BASE_INTENSITY;
};
