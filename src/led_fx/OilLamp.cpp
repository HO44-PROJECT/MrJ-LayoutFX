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
            }

            // Switch phase when duration elapsed.
            if (millis() - phaseStart >= phaseDuration)
            {
                stablePhase = !stablePhase;
                phaseStart = millis();
                phaseDuration = stablePhase
                    ? random(OIL_LAMP_STABLE_MIN_MS, OIL_LAMP_STABLE_MAX_MS)
                    : random(OIL_LAMP_FLICKER_MIN_MS, OIL_LAMP_FLICKER_MAX_MS);
            }

            if (stablePhase)
            {
                // Stable: near-constant brightness with tiny variation.
                intensity = OIL_LAMP_STABLE_INTENSITY + random(-OIL_LAMP_STABLE_VARIATION, OIL_LAMP_STABLE_VARIATION + 1);
                intensity = constrain(intensity, 0, OIL_LAMP_MAX_PWM);
                simulatePWM(_pin, intensity, OIL_LAMP_PWM_PERIOD_US);
                COROUTINE_DELAY(OIL_LAMP_STABLE_STEP_MS);
            }
            else
            {
                // Flicker: rapid random oscillation with occasional surge.
                intensity = random(OIL_LAMP_MIN_INTENSITY, OIL_LAMP_MAX_INTENSITY + 1);
                if (random(OIL_LAMP_SURGE_PROBABILITY) < OIL_LAMP_SURGE_CHANCE)
                    intensity = min(OIL_LAMP_MAX_PWM, intensity + (int)random(OIL_LAMP_SURGE_BOOST));
                simulatePWM(_pin, intensity, OIL_LAMP_PWM_PERIOD_US);
                COROUTINE_DELAY(OIL_LAMP_BASE_DELAY_MS);
            }

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