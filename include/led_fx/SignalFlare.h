/**
 * @file SignalFlare.h
 * @brief Defines the `SignalFlare` class for a railway signal flare effect.
 *
 * This class simulates a railway signal flare with rapid ignition, intense burning with strong flickering, and quick burnout. It inherits from `LedPerpetualEffect` to manage a single LED pin using a coroutine-based state machine with automatic burnout.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-07
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

// Signal flare effect configuration constants
#define SIGNALFLARE_PWM_PERIOD_US 10000           ///< PWM period in microseconds for brightness control (100 Hz).
#define SIGNALFLARE_IGNITION_DURATION_MS 500      ///< Duration of ignition phase in milliseconds.
#define SIGNALFLARE_BURNING_DURATION_MS 2000      ///< Duration of burning phase in milliseconds.
#define SIGNALFLARE_BURNOUT_DURATION_MS 800       ///< Duration of burnout phase in milliseconds.
#define SIGNALFLARE_IGNITION_MIN_BRIGHTNESS 50    ///< Minimum brightness during ignition phase (0–255).
#define SIGNALFLARE_IGNITION_MAX_BRIGHTNESS 150   ///< Maximum brightness during ignition phase (0–255).
#define SIGNALFLARE_BURNING_MIN_BRIGHTNESS 180    ///< Minimum brightness during burning phase (0–255).
#define SIGNALFLARE_BURNING_TARGET_BRIGHTNESS 230 ///< Target brightness during burning phase (0–255).
#define SIGNALFLARE_BURNING_MIN_VARIATION -50     ///< Minimum brightness variation in burning phase (±50).
#define SIGNALFLARE_BURNING_MAX_VARIATION 50      ///< Maximum brightness variation in burning phase (±50).
#define SIGNALFLARE_BURNOUT_STEP_MS 50            ///< Interval for brightness steps in burnout phase (ms).
#define SIGNALFLARE_BURNOUT_MIN_VARIATION -30     ///< Minimum brightness variation in burnout phase (±30).
#define SIGNALFLARE_BURNOUT_MAX_VARIATION 30      ///< Maximum brightness variation in burnout phase (±30).
#define SIGNALFLARE_OFF_THRESHOLD 10              ///< Brightness threshold for transitioning to OFF state (0–255).

/**
 * @class SignalFlare
 * @brief Simulates a railway signal flare with intense flickering and burnout.
 *
 * This class extends `LedPerpetualEffect` to manage a single LED pin using a coroutine-based state machine, implementing ignition, burning, and burnout phases with automatic transition to OFF state.
 */
class SignalFlare : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherits constructors from the base `LedEffect` class.

    static const STATE_TYPE IGNITION = NEXT_NON_STABLE;    ///< Fast, bright flickers to simulate flare ignition.
    static const STATE_TYPE BURNING = NEXT_NON_STABLE - 1; ///< Intense flickering to mimic chemical burning.
    static const STATE_TYPE BURNOUT = NEXT_NON_STABLE - 2; ///< Rapid decrease to simulate dying embers.
    /**
     * @brief Retrieves the device name for identification.
     * @return The C-string "SignalFlare".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("SignalFlare");
    }

    /**
     * @brief Executes the coroutine for the signal flare effect.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    uint32_t startTime = 0;  ///< Timestamp for phase transitions (ms).
    uint32_t timerStart = 0; ///< Timestamp for coroutine delay management (ms).
    int16_t brightness = 0;  ///< Current brightness level (0–255).
};
