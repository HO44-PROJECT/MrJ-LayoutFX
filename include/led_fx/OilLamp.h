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
#define OIL_LAMP_STABLE_MIN_MS 2000          ///< Minimum duration of a stable phase in ms.
#define OIL_LAMP_STABLE_MAX_MS 6000          ///< Maximum duration of a stable phase in ms.

// Flicker phase — lamp flickers briefly.
// Same target/current split as Torch: targetIntensity is redrawn every
// OIL_LAMP_TARGET_UPDATE_MS, but the PWM cycle runs continuously every
// OIL_LAMP_PWM_PERIOD_US (~20ms) and currentIntensity glides toward the
// target by OIL_LAMP_SMOOTHING_FACTOR each cycle. A step applied only once
// per 150ms, with nothing updating in between, is what reads as a strobe —
// the PWM refresh rate itself must carry the transition (#80).
#define OIL_LAMP_MIN_INTENSITY 45            ///< Floor for target intensity during flicker phase (0–255).
#define OIL_LAMP_MAX_INTENSITY 75            ///< Ceiling for target intensity during flicker phase (0–255).
#define OIL_LAMP_TARGET_UPDATE_MS 150        ///< Interval between target intensity redraws in flicker phase.
#define OIL_LAMP_SMOOTHING_FACTOR 0.15f      ///< Weight of the target in the per-cycle glide (0.0–1.0).
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
    float currentIntensity = 0.0f;   ///< Current LED intensity, glides toward targetIntensity each PWM cycle (0–255).
    float targetIntensity = 0.0f;    ///< Target intensity, redrawn periodically (0–255).
    uint32_t lastTargetUpdate = 0;   ///< Timestamp (ms) of the last target intensity redraw.
    uint32_t phaseStart = 0;         ///< Timestamp (ms) when the current phase started.
    uint32_t phaseDuration = 0;      ///< Duration (ms) of the current phase.
    bool stablePhase = true;         ///< true = stable phase, false = flicker phase.
};
