/**
 * @file TrainHeadLamp.h
 * @brief Defines the `TrainHeadLamp` class for simulating a locomotive headlamp.
 *
 * This class simulates a locomotive headlamp with a gradual warmup, a stable intense beam,
 * and a gradual extinction, mimicking the behavior of an old headlamp.
 * It inherits from `LedEffect` to control a single LED pin using a coroutine-based state machine.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

// Train headlamp effect configuration constants
#define TRAINHEADLAMP_WARMUP_DURATION_MS 1000         ///< Duration of the warmup phase in milliseconds.
#define TRAINHEADLAMP_BRIGHTENING_DURATION_MS 1500    ///< Duration of the gradual brightness increase in milliseconds.
#define TRAINHEADLAMP_STABLE_MIN_BRIGHTNESS 240       ///< Minimum brightness of the stable beam (0-255).
#define TRAINHEADLAMP_STABLE_MAX_BRIGHTNESS 255       ///< Maximum brightness of the stable beam (0-255).
#define TRAINHEADLAMP_STABLE_VARIATION 5              ///< Brightness variation for stable beam flicker (±5).
#define TRAINHEADLAMP_EXTINCTION_DURATION_MS 2000     ///< Duration of the gradual extinction in milliseconds.
#define TRAINHEADLAMP_EXTINCTION_VARIATION 10         ///< Brightness variation for flicker during extinction (±10).
#define TRAINHEADLAMP_PWM_PERIOD_US 10000             ///< PWM period in microseconds (100Hz) for brightness control.
#define TRAINHEADLAMP_MAX_INTENSITY 255               ///< Maximum brightness level for PWM (0-255, 8-bit).
#define TRAINHEADLAMP_BRIGHTENING_STEP_MS 10          ///< Interval for brightness updates during brightening in milliseconds.
#define TRAINHEADLAMP_WARMUP_MAX_BRIGHTNESS 80        ///< Maximum brightness during warmup phase (0-80).
#define TRAINHEADLAMP_WARMUP_FLICKER_MIN_DELAY_MS 200 ///< Minimum delay between flickers in warmup phase in milliseconds.
#define TRAINHEADLAMP_WARMUP_FLICKER_MAX_DELAY_MS 400 ///< Maximum delay between flickers in warmup phase in milliseconds.

/**
 * @class TrainHeadLamp
 * @brief Simulates a locomotive headlamp.
 *
 * This class extends `LedEffect` to manage a single LED pin using a coroutine-based state machine,
 * implementing the various phases of a train headlamp.
 */
class TrainHeadLamp : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherits constructors from the base `LedEffect` class.

    static const STATE_TYPE WARMUP = NEXT_NON_STABLE;          ///< Warmup phase with gradual flicker.
    static const STATE_TYPE BRIGHTENING = NEXT_NON_STABLE - 1; ///< Linear brightness increase phase.
    static const STATE_TYPE STABLE_BEAM = NEXT_STABLE;         ///< Stable intense beam with slight flicker.

    /**
     * @brief Retrieves the device name for identification.
     * @return The C-string "TrainHeadLamp".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Train Head Lamp");
    }

    /**
     * @brief Executes the coroutine for the train headlamp effect.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    unsigned long stateStartTime = 0; ///< Timestamp (ms) of the current state start.
    uint8_t brightness = 0;           ///< Current brightness level (0-255).
    unsigned long elapsedTime;        ///< Elapsed time for state transitions (ms).

private:
    uint32_t timerStart = 0; ///< Timer for coroutine delay management (ms).
};
