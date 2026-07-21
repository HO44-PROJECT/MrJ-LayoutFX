/**
 * @file GasLampDefect.h
 * @brief Defines the GasLampDefect class for a gas lamp effect with rare malfunctions.
 *
 * Same ignition, unstable flicker, brightening, stable flame, and extinction phases as
 * GasLamp, but the stable flame occasionally suffers a rare, brief malfunction (a short
 * dark glitch), in the spirit of DefectLamp — far rarer and shorter than DefectLamp's own
 * flicker/outage cycling, which is untouched by this effect.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

/**
 * @class GasLampDefect
 * @brief Simulates a gas lamp with ignition, flicker, stable flame, and rare malfunctions.
 *
 * Extends LedEffect to manage a single LED pin using a coroutine-based state machine,
 * identical to GasLamp except for one extra MALFUNCTION phase reached rarely from
 * STABLE_FLAME.
 */
class GasLampDefect : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    static const STATE_TYPE IGNITION = NEXT_NON_STABLE;            ///< Small irregular flickers during startup.
    static const STATE_TYPE INITIAL_FLICKER = NEXT_NON_STABLE - 1; ///< Larger, unstable flickering.
    static const STATE_TYPE BRIGHTENING = NEXT_NON_STABLE - 2;     ///< Gradual increase to maximum intensity.
    static const STATE_TYPE MALFUNCTION = NEXT_NON_STABLE - 3;     ///< Rare, brief dark glitch during stable flame.

    static const STATE_TYPE STABLE_FLAME = NEXT_STABLE;            ///< Continuous subtle flickering.

    /**
     * @brief Gets the device name for identification.
     * @return "GasLampDefect" for use in debugging or system logs.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("GasLampDefect");
    }

    /**
     * @brief Runs the coroutine for the defective gas lamp effect.
     *
     * Manages ignition, initial flicker, brightening, stable flame (with rare
     * malfunctions), and extinction phases based on desired state.
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
        case MALFUNCTION:
            return 'x';
        }
        return LedEffect::statusChar();
    }

protected:
    uint32_t startTime = 0;  ///< Timestamp (ms) for tracking phase transitions.
    uint8_t brightness = 0;  ///< Current LED brightness level (0-255).
    uint16_t delayMs = 0; // Delay for COROUTINE_DELAY
    uint32_t currentTime = millis();

    uint16_t flickerInterval = 0;

    uint8_t preMalfunctionBrightness = 0; ///< Stable-flame brightness to restore after a malfunction.
    uint16_t malfunctionDurationMs = 0;   ///< Duration of the current malfunction (ms).
};
