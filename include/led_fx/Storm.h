/**
 * @file Storm.h
 * @brief Defines the `Storm` class for simulating a lightning storm effect.
 *
 * This class simulates a lightning storm with ambient glow, intense flash bursts,
 * and calm blackout periods. It inherits from `LedEffect` to control a single
 * LED pin using a coroutine-based state machine.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

// Storm effect configuration constants
#define STORM_MAX_INTENSITY 255         ///< Maximum brightness for lightning flashes (0–255, 8-bit PWM).
#define STORM_FLASH_CHANCE_PERCENT 4    ///< Probability of starting a lightning flash burst (0–100%, 4% per loop).
#define STORM_FLASH_COUNT_MIN 1         ///< Minimum number of flashes in a burst (1–5).
#define STORM_FLASH_COUNT_MAX 5         ///< Maximum number of flashes in a burst (1–5).
#define STORM_FLASH_DURATION_MIN_MS 20  ///< Minimum duration of a flash in milliseconds.
#define STORM_FLASH_DURATION_MAX_MS 80  ///< Maximum duration of a flash in milliseconds.
#define STORM_FLASH_PAUSE_MIN_MS 30     ///< Minimum pause duration between flashes in a burst in milliseconds.
#define STORM_FLASH_PAUSE_MAX_MS 150    ///< Maximum pause duration between flashes in a burst in milliseconds.
#define STORM_DIM_INTENSITY_MIN 2       ///< Minimum glow intensity during idle or pause (0–255).
#define STORM_DIM_INTENSITY_MAX 15      ///< Maximum glow intensity during idle or pause (0–255).
#define STORM_CALM_CHANCE_PERCENT 2     ///< Probability of entering a calm blackout period (0–100%, 2% per loop).
#define STORM_CALM_DURATION_MIN_MS 2000 ///< Minimum duration of a calm blackout period in milliseconds.
#define STORM_CALM_DURATION_MAX_MS 8000 ///< Maximum duration of a calm blackout period in milliseconds.
#define STORM_PWM_PERIOD_US 10000       ///< PWM period in microseconds (100 Hz) for brightness control.
#define STORM_CHANCE_PROBABILITY 100    ///< Probability range for chance evaluations (0–100).
#define STORM_MS_TO_US 1000             ///< Conversion factor from milliseconds to microseconds.

/**
 * @class Storm
 * @brief Simulates a lightning storm with glow, flashes, and blackout periods.
 *
 * This class extends `LedEffect` to manage a single LED pin using a coroutine-based
 * state machine, implementing the phases defined in `StormInternalState`.
 */
class Storm : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherits constructors from the base class `LedEffect`.

    static const STATE_TYPE RUN_IDLE = NEXT_NON_STABLE;      ///< Subtle glow or dark state between flash bursts.
    static const STATE_TYPE RUN_FLASH = NEXT_NON_STABLE - 1; ///< Intense lightning flash state.
    static const STATE_TYPE RUN_PAUSE = NEXT_STABLE;         ///< Brief pause between flashes within a burst.
    static const STATE_TYPE RUN_CALM = NEXT_STABLE + 1;      ///< Complete blackout during calm periods.

    /**
     * @brief Retrieves the device name for identification.
     * @return The C-string "Storm" for debugging or logging.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Storm");
    }

    /**
     * @brief Executes the coroutine for the lightning storm effect.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    uint8_t totalFlashes = 0;        ///< Total number of flashes in the current burst (1–5).
    uint8_t flashIndex = 0;          ///< Current flash index within the burst (0 to totalFlashes-1).
    uint16_t currentDuration = 0;    ///< Duration of the current flash or pause in milliseconds.
    unsigned long calmStartTime = 0; ///< Timestamp (ms) for tracking calm blackout periods.
};
