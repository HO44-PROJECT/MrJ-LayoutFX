/**
 * @file NeonSign.cpp
 * @brief Implements the NeonSign class for a vintage neon sign effect.
 *
 * This file implements the coroutine-based logic for a non-perpetual neon sign effect,
 * including flickering startup, smooth brightening, stable glow with occasional buzz-like drops,
 * and stuttering extinction.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-06
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/NeonSign.h"

/**
 * @brief Executes the coroutine for the neon sign effect.
 * @return Coroutine state from AceRoutine (0 for success).
 */
int NeonSign::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a change in the desired state.
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

                // Start the ignition phase.
                setState(IGNITION);
                startTime = millis();
                brightness = 0;
            }

            // Process the internal state machine for ON_STATE.
            switch (getState())
            {
            case IGNITION:
                // Simulate rapid, irregular flickers during startup (0-100).
                brightness = random(NEONSIGN_IGNITION_MIN_BRIGHTNESS, NEONSIGN_IGNITION_MAX_BRIGHTNESS);
                simulatePWM(_pin, brightness, NEONSIGN_PWM_PERIOD_US);
                COROUTINE_DELAY(random(NEONSIGN_IGNITION_FLICKER_MIN_DELAY_MS, NEONSIGN_IGNITION_FLICKER_MAX_DELAY_MS));
                // Transition to BRIGHTENING after ignition duration.
                if (millis() - startTime > NEONSIGN_IGNITION_DURATION_MS)
                {
                    setState(BRIGHTENING);
                    startTime = millis();
                    brightness = NEONSIGN_IGNITION_MAX_BRIGHTNESS; // Start from 100.
                }
                break;

            case BRIGHTENING:
                // Gradually increase brightness from 100 to 255 with 1-unit steps every 10 ms.
                if (millis() - startTime >= NEONSIGN_BRIGHTENING_STEP_MS)
                {
                    brightness += NEONSIGN_BRIGHTENING_INCREMENT;
                    brightness = constrain(brightness, NEONSIGN_IGNITION_MAX_BRIGHTNESS, NEONSIGN_MAX_INTENSITY);
                    startTime = millis();
                }
                simulatePWM(_pin, brightness, NEONSIGN_PWM_PERIOD_US);
                COROUTINE_DELAY(NEONSIGN_BRIGHTENING_STEP_MS);
                // Transition to STABLE_GLOW when maximum brightness is reached.
                if (brightness >= NEONSIGN_MAX_INTENSITY)
                {
                    setState(STABLE_GLOW);
                    startTime = millis();
                }
                break;

            case STABLE_GLOW:
                // Simulate stable glow with 1% chance of buzz-like drops to 150 every 10 ms.
                brightness = NEONSIGN_TARGET_STABLE_INTENSITY;
                if (random(100) < NEONSIGN_BUZZ_CHANCE_PERCENT)
                {
                    brightness = NEONSIGN_BUZZ_DROP_BRIGHTNESS;
                    simulatePWM(_pin, brightness, NEONSIGN_PWM_PERIOD_US);
                }
                else
                {
                    brightness = NEONSIGN_MAX_INTENSITY;
                    simulatePWM(_pin, brightness, NEONSIGN_PWM_PERIOD_US);
                }
                COROUTINE_DELAY_MILLIS(timerStart, NEONSIGN_STABLE_LIGHT_INTERVAL_MS);
                break;
            }
            break;

        case OFF_STATE:
            // Gradually decrease brightness with 20% chance of skipping steps for stuttering effect.
            if (getState() == INIT_STATE)
            {
                setState(RUN_TRANSIT_STATE);
                startTime = millis();
                brightness = NEONSIGN_MAX_INTENSITY;
            }
            if (millis() - startTime >= NEONSIGN_EXTINCTION_STEP_MS)
            {
                // Simulate stuttering with random skips.
                if (random(100) < NEONSIGN_EXTINCTION_SKIP_CHANCE_PERCENT)
                {
                    COROUTINE_YIELD();
                }
                else
                {
                    brightness -= (NEONSIGN_MAX_INTENSITY * NEONSIGN_EXTINCTION_STEP_MS) / NEONSIGN_EXTINCTION_DURATION_MS;
                    brightness = constrain(brightness, 0, NEONSIGN_MAX_INTENSITY);
                    startTime = millis();
                }
            }
            simulatePWM(_pin, brightness, NEONSIGN_PWM_PERIOD_US);
            // Transition to OFF_STATE when brightness reaches zero.
            if (brightness <= 0)
            {
                outputInactive(_pin);
                setState(OFF_STATE);
            }
            break;
        }
    }

    return 0; // Success.
}