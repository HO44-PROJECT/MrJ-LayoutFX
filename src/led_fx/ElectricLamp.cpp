/**
 * @file ElectricLamp.cpp
 * @brief Implements the `ElectricLamp` class for an electric lamppost effect.
 *
 * This file contains the coroutine-based logic for simulating an old electric lamppost
 * with ignition, brightening, stable light, and extinction phases, using random brightness
 * variations and non-blocking delays for realism.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-05
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#include "led_fx/ElectricLamp.h"

/**
 * @brief Executes the coroutine for the electric lamp effect.
 *
 * This coroutine manages the lamp’s phases: ignition with random flickers (0–50 brightness),
 * brightening to maximum intensity (255), stable light with subtle flicker (230–255),
 * and extinction with gradual dimming and flicker (±5). It responds to target state changes
 * (ON_STATE or OFF_STATE) using a non-blocking state machine.
 * @return 0 on success, per AceRoutine coroutine state definitions.
 */
int ElectricLamp::runCoroutine()
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

                // Start ignition phase with random flickers.
                setState(IGNITION);
                startTime = millis();
                brightness = 0;
            }

            // Process internal states for ON_STATE.
            switch (getState())
            {
            case IGNITION:
                // Simulate filament warmup with random flickers (0–50 brightness).
                brightness = random(ELECTRICLAMP_IGNITION_MIN_BRIGHTNESS, ELECTRICLAMP_IGNITION_MAX_BRIGHTNESS + 1);
                simulatePWM(_pin, brightness, ELECTRICLAMP_PWM_PERIOD_US);
                COROUTINE_DELAY(random(ELECTRICLAMP_IGNITION_FLICKER_MIN_DELAY_MS, ELECTRICLAMP_IGNITION_FLICKER_MAX_DELAY_MS + 1)); // Random delay (100–300ms).
                // Transition to BRIGHTENING after 500ms ignition.
                if (millis() - startTime > ELECTRICLAMP_IGNITION_DURATION_MS)
                {
                    setState(BRIGHTENING);
                    startTime = millis();
                    brightness = 0;
                }
                break;

            case BRIGHTENING:
                // Gradually increase brightness to 255 over 1000ms.
                if (millis() - startTime > ELECTRICLAMP_BRIGHTENING_STEP_MS)
                {
                    brightness += (ELECTRICLAMP_MAX_INTENSITY * ELECTRICLAMP_BRIGHTENING_STEP_MS) / ELECTRICLAMP_BRIGHTENING_DURATION_MS;
                    startTime = millis();
                }
                simulatePWM(_pin, brightness, ELECTRICLAMP_PWM_PERIOD_US);
                // Transition to STABLE_LIGHT when maximum brightness (255) is reached.
                if (brightness >= ELECTRICLAMP_MAX_INTENSITY)
                {
                    setState(STABLE_LIGHT);
                    startTime = millis();
                }
                break;

            case STABLE_LIGHT:
                // Simulate stable light with subtle flicker around 240 (±10 to ±15).
                brightness = ELECTRICLAMP_TARGET_STABLE_INTENSITY + random(ELECTRICLAMP_STABLE_FLICKER_MIN_VARIATION, ELECTRICLAMP_STABLE_FLICKER_MAX_VARIATION + 1);
                brightness = constrain(brightness, ELECTRICLAMP_STABLE_MIN_INTENSITY, ELECTRICLAMP_MAX_INTENSITY);
                simulatePWM(_pin, brightness, ELECTRICLAMP_PWM_PERIOD_US);

                // Transition to stable state after 1000ms stabilization period.
                if (millis() - startTime > ELECTRICLAMP_STABLE_LIGHT_INTERVAL_MS)
                {
                    outputActive(_pin);
                    setState(RUN_STABLE_STATE);
                }
                break;
            }
            break;

        case OFF_STATE:
            // Handle transition to OFF with gradual extinction.
            if (getState() == INIT_STATE)
            {
                setState(RUN_TRANSIT_STATE); // Mark device as busy during extinction.
                startTime = millis();
            }
            // Gradually decrease brightness with ±5 flicker over 1000ms.
            if (millis() - startTime > ELECTRICLAMP_EXTINCTION_STEP_MS)
            {
                brightness -= (ELECTRICLAMP_MAX_INTENSITY * ELECTRICLAMP_EXTINCTION_STEP_MS) / ELECTRICLAMP_EXTINCTION_DURATION_MS;
                brightness += random(ELECTRICLAMP_EXTINCTION_FLICKER_MIN_VARIATION, ELECTRICLAMP_EXTINCTION_FLICKER_MAX_VARIATION + 1);
                brightness = constrain(brightness, 0, ELECTRICLAMP_MAX_INTENSITY);
                startTime = millis();
            }
            simulatePWM(_pin, brightness, ELECTRICLAMP_PWM_PERIOD_US);
            // Transition to OFF_STATE when brightness falls below threshold (10).
            if (brightness <= ELECTRICLAMP_OFF_THRESHOLD)
            {
                outputInactive(_pin);
                setState(OFF_STATE);
            }
            break;
        }
    }

    return 0; // Success.
}