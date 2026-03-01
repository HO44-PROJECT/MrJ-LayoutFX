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
                // Initialize the output pin and handle potential failures.
                if (!handlePinInitFailure())
                    continue;
                setState(RUN_STABLE_STATE); // Mark device as busy in stable operation.
            }

            // Set random baseline intensity for flicker (30–70).
            intensity = random(OIL_LAMP_MIN_INTENSITY, OIL_LAMP_MAX_INTENSITY + 1);

            // Apply occasional bright surge with 4% probability.
            if (random(OIL_LAMP_SURGE_PROBABILITY) < OIL_LAMP_SURGE_CHANCE)
            {
                // Increase intensity by 30, capped at 255 for a surge effect.
                intensity = min(OIL_LAMP_MAX_PWM, intensity + (int)random(OIL_LAMP_SURGE_BOOST));
            }

            // Output PWM signal with current intensity at 50 Hz.
            simulatePWM(_pin, intensity, OIL_LAMP_PWM_PERIOD_US);

            // Pause for 80ms to create a slow, breathing rhythm.
            COROUTINE_DELAY(OIL_LAMP_BASE_DELAY_MS);

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