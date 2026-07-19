/**
 * @file ElectricLamp.h
 * @brief Defines the `ElectricLamp` class for an electric lamppost effect.
 *
 * This class simulates the startup, stable illumination, and extinction of an old
 * electric lamppost using staged brightness and subtle random intensity variations.
 * It inherits from `LedPerpetualEffect` to manage a single output pin using a
 * coroutine-based state machine.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-05
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

// Electric lamp effect configuration constants
#define ELECTRICLAMP_PWM_PERIOD_US 10000                 ///< PWM period in microseconds for brightness control (100 Hz).
#define ELECTRICLAMP_MAX_INTENSITY 255                   ///< Maximum brightness level for PWM (0–255, 8-bit).
#define ELECTRICLAMP_TARGET_STABLE_INTENSITY 240         ///< Target brightness in stable light phase (0–255).
#define ELECTRICLAMP_STABLE_MIN_INTENSITY 230            ///< Minimum brightness during stable light phase (0–255).
#define ELECTRICLAMP_IGNITION_DURATION_MS 500            ///< Duration of the ignition phase in milliseconds.
#define ELECTRICLAMP_BRIGHTENING_DURATION_MS 1000        ///< Duration of the brightening phase in milliseconds.
#define ELECTRICLAMP_EXTINCTION_DURATION_MS 1000         ///< Duration of the extinction phase in milliseconds.
#define ELECTRICLAMP_IGNITION_MIN_BRIGHTNESS 0           ///< Minimum brightness during ignition phase (0–255).
#define ELECTRICLAMP_IGNITION_MAX_BRIGHTNESS 50          ///< Maximum brightness during ignition phase (0–255).
#define ELECTRICLAMP_BRIGHTENING_STEP_MS 50              ///< Interval for brightness increase steps in brightening phase (ms).
#define ELECTRICLAMP_EXTINCTION_STEP_MS 50               ///< Interval for brightness decrease steps in extinction phase (ms).
#define ELECTRICLAMP_STABLE_LIGHT_INTERVAL_MS 1000       ///< Interval for flicker updates in stable light phase (ms).
#define ELECTRICLAMP_IGNITION_FLICKER_MIN_DELAY_MS 100   ///< Minimum delay between flickers in ignition phase (ms).
#define ELECTRICLAMP_IGNITION_FLICKER_MAX_DELAY_MS 300   ///< Maximum delay between flickers in ignition phase (ms).
#define ELECTRICLAMP_STABLE_FLICKER_MIN_VARIATION -10    ///< Minimum brightness variation in stable light phase (±10).
#define ELECTRICLAMP_STABLE_FLICKER_MAX_VARIATION 15     ///< Maximum brightness variation in stable light phase (±15).
#define ELECTRICLAMP_EXTINCTION_FLICKER_MIN_VARIATION -5 ///< Minimum brightness variation in extinction phase (±5).
#define ELECTRICLAMP_EXTINCTION_FLICKER_MAX_VARIATION 5  ///< Maximum brightness variation in extinction phase (±5).
#define ELECTRICLAMP_OFF_THRESHOLD 10                    ///< Brightness threshold for transitioning to OFF state (0–255).

/**
 * @class ElectricLamp
 * @brief Simulates an electric lamppost with startup, illumination, and extinction.
 *
 * This class extends `LedPerpetualEffect` to manage a single LED pin using a
 * coroutine-based state machine, implementing ignition, brightening, and stable phases.
 */
class ElectricLamp : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherits constructors from the base `LedEffect` class.

    static const STATE_TYPE IGNITION = NEXT_NON_STABLE;        ///< Brief flickers during startup to simulate filament warmup.
    static const STATE_TYPE BRIGHTENING = NEXT_NON_STABLE - 1; ///< Gradual increase to maximum intensity.
    static const STATE_TYPE STABLE_LIGHT = NEXT_STABLE;        ///< Continuous subtle flickering for stable illumination.

    /**
     * @brief Retrieves the device name for identification.
     * @return The C-string "ElectricLamp".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("ElectricLamp");
    }

    /**
     * @brief Executes the coroutine for the electric lamp effect.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    uint32_t startTime = 0;  ///< Timestamp for phase transitions (ms).
    uint32_t timerStart = 0; ///< Timestamp for coroutine delay management (ms).
    int16_t brightness = 0;  ///< Current brightness level (0–255).
};
