/**
 * @file TrainHeadLamp.cpp
 * @brief Implements the `TrainHeadLamp` class for simulating a locomotive headlamp.
 *
 * This file contains the state transition and timing logic for the train headlamp effect.
 * It simulates a warmup phase, a stable beam, and a gradual extinction, managed by a non-blocking coroutine.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/TrainHeadLamp.h"

/**
 * @brief Executes the coroutine for the train headlamp effect.
 *
 * This method manages transitions between the headlamp’s phases using a non-blocking state machine for smooth operation.
 * @return 0 on success, per AceRoutine coroutine state definitions.
 */
int TrainHeadLamp::runCoroutine()
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
                setState(WARMUP);          // Start warmup phase, device is busy.
                stateStartTime = millis(); // Record start time for phase timing.
            }

            switch (getState())
            {
            case WARMUP:
                // Simulate filament heating with random flickers (0-80 brightness, 200-400ms delays).
                simulatePWM(_pin, random(0, TRAINHEADLAMP_WARMUP_MAX_BRIGHTNESS + 1), TRAINHEADLAMP_PWM_PERIOD_US);
                COROUTINE_DELAY(random(TRAINHEADLAMP_WARMUP_FLICKER_MIN_DELAY_MS, TRAINHEADLAMP_WARMUP_FLICKER_MAX_DELAY_MS + 1));
                // Check if warmup duration (1000ms) has elapsed.
                if (millis() - stateStartTime >= TRAINHEADLAMP_WARMUP_DURATION_MS)
                {
                    // Transition to BRIGHTENING phase.
                    stateStartTime = millis();
                    setState(BRIGHTENING);
                }
                break;

            case BRIGHTENING:
            {
                // Calculate elapsed time since brightening started.
                elapsedTime = millis() - stateStartTime;
                if (elapsedTime < TRAINHEADLAMP_BRIGHTENING_DURATION_MS)
                {
                    // Linearly increase brightness from 0 to 255 over 1500ms.
                    brightness = (elapsedTime * TRAINHEADLAMP_MAX_INTENSITY) / TRAINHEADLAMP_BRIGHTENING_DURATION_MS;
                    simulatePWM(_pin, brightness, TRAINHEADLAMP_PWM_PERIOD_US);
                    COROUTINE_DELAY(TRAINHEADLAMP_BRIGHTENING_STEP_MS); // Update every 10ms for smooth ramp-up.
                }
                else
                {
                    // Transition to STABLE_BEAM when maximum brightness is reached.
                    setState(STABLE_BEAM);
                }
            }
            break;

            case STABLE_BEAM:
                // Maintain stable beam with slight flicker (240-255 brightness).
                simulatePWM(_pin, random(TRAINHEADLAMP_STABLE_MIN_BRIGHTNESS, TRAINHEADLAMP_STABLE_MAX_BRIGHTNESS + 1), TRAINHEADLAMP_PWM_PERIOD_US);
                COROUTINE_DELAY(TRAINHEADLAMP_BRIGHTENING_STEP_MS); // Update every 10ms for consistent flicker.
                break;
            }
            break;

        case OFF_STATE:
            // Handle transition to OFF with gradual extinction.
            if (getState() == INIT_STATE)
            {
                // Initialize extinction phase.
                setState(RUN_TRANSIT_STATE);
                brightness = TRAINHEADLAMP_MAX_INTENSITY; // Start from maximum brightness.
                stateStartTime = millis();
            }
            elapsedTime = millis() - stateStartTime;
            if (elapsedTime < TRAINHEADLAMP_EXTINCTION_DURATION_MS)
            {
                // Gradually decrease brightness over 2000ms with ±10 flicker for cooling effect.
                brightness = TRAINHEADLAMP_MAX_INTENSITY - (elapsedTime * TRAINHEADLAMP_MAX_INTENSITY) / TRAINHEADLAMP_EXTINCTION_DURATION_MS;
                brightness = constrain(brightness + random(-TRAINHEADLAMP_EXTINCTION_VARIATION, TRAINHEADLAMP_EXTINCTION_VARIATION + 1), 0, TRAINHEADLAMP_MAX_INTENSITY);
                simulatePWM(_pin, brightness, TRAINHEADLAMP_PWM_PERIOD_US);
                COROUTINE_DELAY(TRAINHEADLAMP_BRIGHTENING_STEP_MS); // Update every 10ms for smooth fade-out.
            }
            else
            {
                // Turn off the LED and set state to OFF.
                outputInactive(_pin);
                setState(OFF_STATE);
            }
            break;
        }
    }
    return 0; // Success.
}