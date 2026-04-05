/**
 * @file OilLamp.h
 * @brief Defines the `OilLamp` class for a flickering oil lamp light effect.
 *
 * This class simulates a flickering oil lamp flame with random intensity fluctuations
 * and occasional bright surges, mimicking a realistic flame behavior. It inherits
 * from `LedPerpetualEffect` to manage a single output pin using a coroutine-based state machine.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

// Oil lamp effect configuration constants
#define OIL_LAMP_PWM_PERIOD_US 20000         ///< PWM period in microseconds (50 Hz) for brightness control.
#define OIL_LAMP_MAX_PWM 255                 ///< Maximum PWM intensity (0–255, 8-bit).

// Stable phase — lamp burns calmly
#define OIL_LAMP_STABLE_INTENSITY 95         ///< Base intensity during stable phase (0–255).
#define OIL_LAMP_STABLE_VARIATION 8          ///< Max random variation (±) during stable phase.
#define OIL_LAMP_STABLE_STEP_MS 150          ///< Delay between brightness steps in stable phase.
#define OIL_LAMP_STABLE_MIN_MS 2000          ///< Minimum duration of a stable phase in ms.
#define OIL_LAMP_STABLE_MAX_MS 6000          ///< Maximum duration of a stable phase in ms.

// Flicker phase — lamp flickers briefly
#define OIL_LAMP_MIN_INTENSITY 30            ///< Minimum intensity during flicker phase (0–255).
#define OIL_LAMP_MAX_INTENSITY 70            ///< Maximum intensity during flicker phase (0–255).
#define OIL_LAMP_BASE_DELAY_MS 80            ///< Delay between intensity updates in flicker phase.
#define OIL_LAMP_FLICKER_MIN_MS 400          ///< Minimum duration of a flicker phase in ms.
#define OIL_LAMP_FLICKER_MAX_MS 1200         ///< Maximum duration of a flicker phase in ms.

// Surge (applies in both phases)
#define OIL_LAMP_SURGE_CHANCE 4              ///< Probability of a bright surge (0–100).
#define OIL_LAMP_SURGE_BOOST 30              ///< Intensity boost during a surge.
#define OIL_LAMP_SURGE_PROBABILITY 100       ///< Probability range for surge evaluation.

/**
 * @class OilLamp
 * @brief Simulates a flickering oil lamp light effect.
 *
 * This class extends `LedPerpetualEffect` to manage a single LED pin using a coroutine-based
 * state machine, implementing random intensity fluctuations with occasional bright surges.
 */
class OilLamp : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherits constructors from the base `LedEffect` class.

    virtual const __FlashStringHelper *getDeviceName() const override { return F("OilLamp"); }

    /**
     * @brief Executes the coroutine for the oil lamp flicker effect.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    int16_t intensity = 0;       ///< Current LED intensity (0–255).
    uint32_t phaseStart = 0;     ///< Timestamp (ms) when the current phase started.
    uint32_t phaseDuration = 0;  ///< Duration (ms) of the current phase.
    bool stablePhase = true;     ///< true = stable phase, false = flicker phase.
};
