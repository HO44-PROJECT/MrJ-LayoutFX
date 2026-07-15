/**
 * @file DefectLamp.cpp
 * @brief Implements the coroutine logic for a defective lamp light effect.
 *
 * This file contains the implementation of the `DefectLamp` class, providing state
 * transition and timing logic for a malfunctioning lamp with random flickers (60–180 intensity),
 * sudden outages (50–300ms), and stable glowing periods (100 intensity). It uses a
 * coroutine for non-blocking operation.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/DefectLamp.h"

/**
 * @brief Executes the coroutine for the defective lamp effect.
 *
 * This coroutine manages random flickers, outages, and stable periods for the ON_STATE
 * with probabilities (10% to flicker, 80% to outage, 8% to stable) and turns off the
 * LED for the OFF_STATE using the inactive state configuration.
 * @return 0 on success, per AceRoutine coroutine state definitions.
 */
int DefectLamp::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a change in the desired state (ON_STATE or OFF_STATE).
        DEVICE_WAIT_STATE_CHANGE(getTargetState());
        DEVICE_APPLY_START_DELAY();

        switch (getTargetState())
        {
        case ON_STATE:
            // Handle transition from OFF to ON.
            if (getState() == INIT_STATE)
            {
                // Initialize the output pin and handle potential failures.
                if (!handlePinInitFailure())
                    continue;
                setState(RUN_STABLE); // Mark device as busy in stable operation.

                offStartTime = 0;                         // Reset outage timestamp.
                offDuration = 0;                          // Reset outage duration.
                intensity = DEFECT_LAMP_STABLE_INTENSITY; // Set initial intensity to 100.
            }

            switch (getState())
            {
            case RUN_STABLE:
                // Output stable intensity (100) at 50 Hz.
                simulatePWM(_pin, DEFECT_LAMP_STABLE_INTENSITY, DEFECT_LAMP_PWM_PERIOD_US);
                // Check for 10% chance to switch to flicker mode.
                if (random(DEFECT_LAMP_CHANCE_PROBABILITY) < DEFECT_LAMP_FLICKER_CHANCE)
                {
                    setState(RUN_FLICKER);
                }
                COROUTINE_DELAY(DEFECT_LAMP_BASE_DELAY_MS); // Delay 50ms between updates.
                break;

            case RUN_FLICKER:
                // Check for 80% chance to trigger a sudden outage (50–300ms).
                if (random(DEFECT_LAMP_CHANCE_PROBABILITY) < DEFECT_LAMP_OFF_CHANCE)
                {
                    setState(RUN_OFF);
                    offStartTime = millis();
                    offDuration = random(DEFECT_LAMP_OFF_MIN_MS, DEFECT_LAMP_OFF_MAX_MS + 1);
                    outputInactive(_pin); // Turn off LED for outage.
                    break;
                }
                // Output random flicker intensity (60–180) at 50 Hz.
                intensity = random(DEFECT_LAMP_FLICKER_MIN, DEFECT_LAMP_FLICKER_MAX + 1);
                simulatePWM(_pin, intensity, DEFECT_LAMP_PWM_PERIOD_US);
                // Check for 8% chance to return to stable mode.
                if (random(DEFECT_LAMP_CHANCE_PROBABILITY) < DEFECT_LAMP_STABLE_CHANCE)
                {
                    setState(RUN_STABLE);
                    intensity = DEFECT_LAMP_STABLE_INTENSITY;
                }
                COROUTINE_DELAY(DEFECT_LAMP_BASE_DELAY_MS); // Delay 50ms between updates.
                break;

            case RUN_OFF:
                // Maintain outage until duration (50–300ms) expires.
                outputInactive(_pin);
                // Return to stable state after outage.
                if (millis() - offStartTime >= offDuration)
                {
                    setState(RUN_STABLE);
                    intensity = DEFECT_LAMP_STABLE_INTENSITY;
                }
                COROUTINE_YIELD(); // Yield during outage to avoid blocking.
                break;
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