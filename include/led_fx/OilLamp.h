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
#define OIL_LAMP_MIN_INTENSITY 30            ///< Minimum intensity for the flicker effect (0–255).
#define OIL_LAMP_MAX_INTENSITY 70            ///< Maximum intensity for the flicker effect (0–255).
#define OIL_LAMP_MAX_PWM 255                 ///< Maximum PWM intensity for the oil lamp effect (0–255, 8-bit).
#define OIL_LAMP_BASE_DELAY_MS 80            ///< Base delay between intensity updates in milliseconds.
#define OIL_LAMP_SURGE_CHANCE 4              ///< Probability of a bright surge occurring (0–100%).
#define OIL_LAMP_SURGE_BOOST 30              ///< Intensity increase during a bright surge (0–255).
#define OIL_LAMP_SURGE_PROBABILITY 100       ///< Probability range for surge chance evaluation (0–100).

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

    virtual const __FlashStringHelper *getDeviceName() const override { return F("Oil Lamp"); }

    /**
     * @brief Executes the coroutine for the oil lamp flicker effect.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    int16_t intensity = 0; ///< Current intensity of the oil lamp flicker (0–255).
};
