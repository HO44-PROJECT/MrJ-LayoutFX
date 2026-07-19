/**
 * @file Storm.cpp
 * @brief Implements the `Storm` class for a lightning storm effect.
 *
 * This file simulates a lightning storm with subtle glow, intense flash bursts,
 * and calm blackout periods using a non-blocking coroutine for realistic timing.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/Storm.h"

/**
 * @brief Executes the coroutine for the lightning storm effect.
 *
 * This coroutine manages subtle glow, intense flash bursts, and calm blackout periods
 * based on the desired state, using non-blocking delays and random variations for realism.
 * @return 0 on success, per AceRoutine coroutine state definitions.
 */
int Storm::runCoroutine()
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
                // Initialize output pin and handle potential failures.
                if (!handlePinInitFailure())
                    continue;
                setState(RUN_IDLE);  // Start in idle state with subtle glow.
                totalFlashes = 0;    // Reset flash burst counter.
                flashIndex = 0;      // Reset flash index.
                currentDuration = 0; // Reset duration tracker.
                calmStartTime = 0;   // Reset calm period timestamp.
            }

            // Process ON state phases.
            switch (getState())
            {
            case RUN_IDLE:
                // Check for 2% chance of entering a calm blackout period (2000–8000ms).
                if (random(STORM_CHANCE_PROBABILITY) < STORM_CALM_CHANCE_PERCENT)
                {
                    calmStartTime = millis();
                    currentDuration = random(STORM_CALM_DURATION_MIN_MS, STORM_CALM_DURATION_MAX_MS);
                    outputInactive(_pin); // Turn off LED for blackout.
                    setState(RUN_CALM);   // Transition to calm state.
                }
                // Check for 4% chance to start a lightning flash burst (1–5 flashes).
                else if (random(STORM_CHANCE_PROBABILITY) < STORM_FLASH_CHANCE_PERCENT)
                {
                    totalFlashes = random(STORM_FLASH_COUNT_MIN, STORM_FLASH_COUNT_MAX + 1);
                    flashIndex = 0; // Start at first flash.
                    currentDuration = random(STORM_FLASH_DURATION_MIN_MS, STORM_FLASH_DURATION_MAX_MS);
                    setState(RUN_FLASH); // Transition to flash state.
                }
                // Maintain subtle background glow (2–15 intensity).
                else
                {
                    simulatePWM(_pin, random(STORM_DIM_INTENSITY_MIN, STORM_DIM_INTENSITY_MAX), STORM_PWM_PERIOD_US);
                }
                break;

            case RUN_FLASH:
                // Produce bright lightning flash at maximum intensity (255).
                simulatePWM(_pin, STORM_MAX_INTENSITY, STORM_PWM_PERIOD_US);
                COROUTINE_DELAY_MICROS(currentDuration * STORM_MS_TO_US); // Delay for flash duration (20–80ms).

                flashIndex++;
                if (flashIndex < totalFlashes)
                {
                    // Pause briefly between flashes (30–150ms).
                    currentDuration = random(STORM_FLASH_PAUSE_MIN_MS, STORM_FLASH_PAUSE_MAX_MS);
                    setState(RUN_PAUSE); // Transition to pause state.
                }
                else
                {
                    // Return to idle state after completing the burst.
                    setState(RUN_IDLE);
                }
                break;

            case RUN_PAUSE:
                // Simulate subtle glow between flashes (2–15 intensity).
                simulatePWM(_pin, random(STORM_DIM_INTENSITY_MIN, STORM_DIM_INTENSITY_MAX), STORM_PWM_PERIOD_US);
                COROUTINE_DELAY_MICROS(currentDuration * STORM_MS_TO_US); // Delay for pause duration (30–150ms).

                // Prepare for the next flash in the burst (20–80ms).
                currentDuration = random(STORM_FLASH_DURATION_MIN_MS, STORM_FLASH_DURATION_MAX_MS);
                setState(RUN_FLASH); // Transition back to flash state.
                break;

            case RUN_CALM:
                // Maintain blackout until calm duration (2000–8000ms) expires.
                COROUTINE_AWAIT(millis() - calmStartTime >= currentDuration);
                // Return to idle state after blackout.
                setState(RUN_IDLE);
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