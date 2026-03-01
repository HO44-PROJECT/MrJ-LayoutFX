/**
 * @file GasLamp.h
 * @brief Defines the GasLamp class for a flickering gas lamp effect.
 *
 * Simulates a vintage gas lamp with ignition, unstable flicker, brightening, and stable flame phases.
 * Inherits from LedEffect to control a single LED pin using a coroutine-based state machine.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-05
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __GASLAMP_H__
#define __GASLAMP_H__

#include "LedEffect.h"

/**
 * @class GasLamp
 * @brief Simulates a gas lamp with ignition, flicker, and stable flame phases.
 *
 * Extends LedEffect to manage a single LED pin using a coroutine-based state machine with a fine-grained extinction effect.
 */
class GasLamp : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    static const STATE_TYPE IGNITION = NEXT_NON_STABLE;            ///< Small irregular flickers during startup.
    static const STATE_TYPE INITIAL_FLICKER = NEXT_NON_STABLE - 1; ///< Larger, unstable flickering.
    static const STATE_TYPE BRIGHTENING = NEXT_NON_STABLE - 2;     ///< Gradual increase to maximum intensity.

    static const STATE_TYPE STABLE_FLAME = NEXT_STABLE;            ///< Continuous subtle flickering.

    /**
     * @brief Gets the device name for identification.
     * @return "GasLamp" for use in debugging or system logs.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Gas Lamp");
    }

    /**
     * @brief Runs the coroutine for the gas lamp effect.
     *
     * Manages ignition, initial flicker, brightening, stable flame, and extinction phases based on desired state.
     * @return 0 on success (AceRoutine coroutine state).
     */
    virtual int runCoroutine() override;

    virtual char statusChar()
    {
        switch (state)
        {
        case IGNITION:
            return 'i';
        case INITIAL_FLICKER:
            return 'f';
        case BRIGHTENING:
            return 'b';
        case STABLE_FLAME:
            return 'F';
        }
        return LedEffect::statusChar();
    }

protected:
    uint32_t startTime = 0;  ///< Timestamp (ms) for tracking phase transitions.
    uint8_t brightness = 0;  ///< Current LED brightness level (0-255).

    uint16_t delayMs = 0; // Delay for COROUTINE_DELAY
    uint32_t currentTime = millis();

    uint16_t flickerInterval = 0;
};

#endif // __GASLAMP_H__