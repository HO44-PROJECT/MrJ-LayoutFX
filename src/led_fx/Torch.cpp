/**
 * @file Torch.cpp
 * @brief Implements the coroutine logic for a flickering torch light effect.
 *
 * This file contains the implementation of the `Torch` class, providing state
 * transition and timing logic for a torch flame with random intensity fluctuations,
 * bright surges, and brief extinction cycles. It uses a coroutine to ensure
 * non-blocking operation, allowing concurrent tasks in the main loop.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/Torch.h"

/**
 * @brief Executes the coroutine for the torch flicker effect.
 *
 * This coroutine manages random flickers, surges, and extinction cycles for the
 * `ON_STATE` to simulate a torch flame. It turns off the LED for the `OFF_STATE`
 * using the inactive state configuration.
 *
 * @return int The coroutine state, as defined by AceRoutine.
 */
int Torch::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a state change request.
        DEVICE_WAIT_STATE_CHANGE(getTargetState());

        switch (getTargetState())
        {
        case ON_STATE:
            // Handle transition from OFF to ON.
            if (getState() == INIT_STATE)
            {
                // Initialize the output pin.
                if (!handlePinInitFailure())
                    continue;
                setState(RUN_STABLE_STATE); // Now state is changing, device is busy.

                currentFlicker = 1.0f;
                targetFlicker = 1.0f;
                lastFlickerUpdate = 0;
                onDuration = 0;
                offDuration = 0;
                extinctionCyclesRemaining = 0;
                waitStartTime = 0;
            }

            // Update target flicker intensity periodically.
            if (millis() - lastFlickerUpdate > FLICKER_UPDATE_INTERVAL)
            {
                lastFlickerUpdate = millis();

                // Apply a bright surge or normal flicker variation.
                if (random(TORCH_PERCENT_RANGE) < FLICKER_SURGE_CHANCE)
                {
                    targetFlicker = 1.0f;
                }
                else
                {
                    float variation = random(FLICKER_VARIATION_MIN, FLICKER_VARIATION_MAX) / 100.0f;
                    targetFlicker = FLICKER_TARGET_BASE + variation;
                    targetFlicker = constrain(targetFlicker, FLICKER_TARGET_MIN, FLICKER_TARGET_MAX);
                }

                // Trigger a rare extinction event.
                if (random(TORCH_PERCENT_RANGE) < FLICKER_EXTINCTION_CHANCE && extinctionCyclesRemaining == 0)
                {
                    extinctionCyclesRemaining = random(EXTINCTION_MIN_COUNT, EXTINCTION_MAX_COUNT + 1);
                }
            }

            if (extinctionCyclesRemaining > 0)
            {
                // Turn off LED for extinction burst.
                outputInactive(_pin);
                waitStartTime = millis();
                COROUTINE_AWAIT(millis() - waitStartTime >= EXTINCTION_BURST_MS);
                extinctionCyclesRemaining--;
            }
            else
            {
                // Smoothly adjust current flicker intensity.
                currentFlicker = (1.0f - SMOOTHING_FACTOR) * currentFlicker + SMOOTHING_FACTOR * targetFlicker;

                // Calculate PWM cycle durations in microseconds.
                onDuration = (uint16_t)(currentFlicker * TORCH_PWM_PERIOD_US);
                offDuration = TORCH_PWM_PERIOD_US - onDuration;

                // Execute PWM cycle.
                outputActive(_pin);
                COROUTINE_DELAY_MICROS(onDuration);
                outputInactive(_pin);
                COROUTINE_DELAY_MICROS(offDuration);
            }

            break;

        case OFF_STATE:
            // Turn off the LED and set the state to OFF.
            outputInactive(_pin);
            setState(OFF_STATE);
            break;
        }
    }

    return 0; // Return value for success.
}