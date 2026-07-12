/**
 * @file OilLamp.cpp
 * @brief Implements the coroutine logic for a flickering oil lamp light effect.
 *
 * This file contains the implementation of the `OilLamp` class, providing state transition
 * and timing logic for an oil lamp light with random intensity fluctuations and occasional
 * bright surges. It uses a coroutine to ensure non-blocking operation, allowing concurrent
 * tasks in the main loop.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/OilLamp.h"

/**
 * @brief Executes the coroutine for the oil lamp flicker effect.
 *
 * This coroutine manages random intensity changes with occasional surges for the ON_STATE
 * to simulate a flickering oil lamp. It turns off the LED for the OFF_STATE using the
 * inactive state configuration.
 * @return 0 on success, per AceRoutine coroutine state definitions.
 */
int OilLamp::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a change in the desired state (ON_STATE or OFF_STATE).
        DEVICE_WAIT_STATE_CHANGE(getTargetState());

        switch (getTargetState())
        {
        case ON_STATE:
            // Handle transition from OFF to ON.
            if (getState() == INIT_STATE)
            {
                if (!handlePinInitFailure())
                    continue;
                setState(RUN_STABLE_STATE);
                stablePhase = true;
                phaseStart = millis();
                phaseDuration = random(OIL_LAMP_STABLE_MIN_MS, OIL_LAMP_STABLE_MAX_MS);
                currentIntensity = OIL_LAMP_STABLE_INTENSITY;
                targetIntensity = OIL_LAMP_STABLE_INTENSITY;
                lastTargetUpdate = 0;
            }

            // Switch phase when duration elapsed.
            if (millis() - phaseStart >= phaseDuration)
            {
                stablePhase = !stablePhase;
                phaseStart = millis();
                phaseDuration = stablePhase
                    ? random(OIL_LAMP_STABLE_MIN_MS, OIL_LAMP_STABLE_MAX_MS)
                    : random(OIL_LAMP_FLICKER_MIN_MS, OIL_LAMP_FLICKER_MAX_MS);
                lastTargetUpdate = 0; // force an immediate target redraw on phase entry
            }

            // Redraw the target intensity periodically — the actual output
            // (currentIntensity) glides toward it every PWM cycle below, it
            // never jumps straight to this value.
            if (millis() - lastTargetUpdate >= OIL_LAMP_TARGET_UPDATE_MS)
            {
                lastTargetUpdate = millis();
                if (stablePhase)
                {
                    // Stable: near-constant brightness with tiny variation.
                    targetIntensity = OIL_LAMP_STABLE_INTENSITY + random(-OIL_LAMP_STABLE_VARIATION, OIL_LAMP_STABLE_VARIATION + 1);
                    targetIntensity = constrain(targetIntensity, 0, (float)OIL_LAMP_MAX_PWM);
                }
                else
                {
                    // Flicker: bounded random walk (each redraw nudges the target
                    // by a step within the flicker range) with an occasional surge.
                    targetIntensity = OIL_LAMP_MIN_INTENSITY + random(OIL_LAMP_MAX_INTENSITY - OIL_LAMP_MIN_INTENSITY + 1);
                    if (random(OIL_LAMP_SURGE_PROBABILITY) < OIL_LAMP_SURGE_CHANCE)
                        targetIntensity = min((float)OIL_LAMP_MAX_PWM, targetIntensity + random(OIL_LAMP_SURGE_BOOST));
                }
            }

            // Glide the output toward the target every PWM cycle (~20ms) — this
            // is what actually removes the strobe: the LED is re-driven far more
            // often than the target changes, so consecutive frames are always
            // close together instead of jumping the full step in one refresh.
            currentIntensity = (1.0f - OIL_LAMP_SMOOTHING_FACTOR) * currentIntensity + OIL_LAMP_SMOOTHING_FACTOR * targetIntensity;
            simulatePWM(_pin, (int16_t)currentIntensity, OIL_LAMP_PWM_PERIOD_US);

            break;

        case OFF_STATE:
            // Turn off the LED and set state to OFF.
            outputInactive(_pin);
            setState(OFF_STATE);
            break;
        }
    }

    return 0; // Success.
}