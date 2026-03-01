/**
 * @file DefectLamp.h
 * @brief Defines the `DefectLamp` class for a defective, flickering lamp effect.
 *
 * This class simulates a malfunctioning lamp with random flickers, sudden brief outages,
 * and stable glowing periods. It inherits from `LedPerpetualEffect` to manage a single
 * output pin using a coroutine-based state machine.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __DEFECTLAMP_H__
#define __DEFECTLAMP_H__

#include "LedEffect.h"

// Defect lamp effect configuration constants
#define DEFECT_LAMP_PWM_PERIOD_US 20000    ///< PWM period in microseconds for brightness control (50 Hz).
#define DEFECT_LAMP_BASE_DELAY_MS 50       ///< Base delay between state updates in milliseconds.
#define DEFECT_LAMP_FLICKER_CHANCE 10      ///< Probability of transitioning from stable to flickering (0–100%, 10% per loop).
#define DEFECT_LAMP_STABLE_CHANCE 8        ///< Probability of transitioning from flickering to stable (0–100%, 8% per loop).
#define DEFECT_LAMP_OFF_CHANCE 80          ///< Probability of a sudden outage during flicker mode (0–100%, 80% per loop).
#define DEFECT_LAMP_STABLE_INTENSITY 100   ///< Brightness level in stable state (0–255).
#define DEFECT_LAMP_FLICKER_MIN 60         ///< Minimum intensity for flicker effect (0–255).
#define DEFECT_LAMP_FLICKER_MAX 180        ///< Maximum intensity for flicker effect (0–255).
#define DEFECT_LAMP_OFF_MIN_MS 50          ///< Minimum duration of a sudden outage in milliseconds.
#define DEFECT_LAMP_OFF_MAX_MS 300         ///< Maximum duration of a sudden outage in milliseconds.
#define DEFECT_LAMP_CHANCE_PROBABILITY 100 ///< Probability range for chance evaluations (0–100).

/**
 * @class DefectLamp
 * @brief Simulates a defective lamp with flickering and outages.
 *
 * This class extends `LedPerpetualEffect` to manage a single LED pin using a
 * coroutine-based state machine, implementing random transitions between stable,
 * flickering, and off states.
 */
class DefectLamp : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherits constructors from the base `LedEffect` class.

    static const STATE_TYPE RUN_STABLE = NEXT_STABLE;      ///< Lamp glows at stable intensity (100).
    static const STATE_TYPE RUN_FLICKER = NEXT_NON_STABLE; ///< Lamp flickers randomly (60–180 intensity).
    static const STATE_TYPE RUN_OFF = NEXT_NON_STABLE - 1; ///< Lamp is temporarily off due to a defect.

    /**
     * @brief Retrieves the device name for identification.
     * @return The C-string "DefectLamp".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("DefectLamp");
    }

    /**
     * @brief Executes the coroutine for the defective lamp effect.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    unsigned long offStartTime = 0;                                              ///< Timestamp for outage start (ms).
    unsigned long offDuration = 0;                                               ///< Duration of the current outage (ms).
    uint8_t intensity = DEFECT_LAMP_STABLE_INTENSITY;                            ///< Current intensity of the lamp (0–255).
};

#endif // __DEFECTLAMP_H__