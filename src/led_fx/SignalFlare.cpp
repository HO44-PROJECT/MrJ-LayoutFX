/**
 * @file SignalFlare.cpp
 * @brief Implements the coroutine logic for a railway signal flare effect.
 *
 * This file contains the implementation of the `SignalFlare` class, simulating a flare with rapid ignition (50–150 brightness, 20–100ms delays), intense burning around 230 (180–255, ±50 flicker), and quick burnout with ±30 flicker over 800ms. It uses a coroutine for non-blocking operation with automatic burnout.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-07
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/SignalFlare.h"

/**
 * @brief Executes the coroutine for the signal flare effect.
 *
 * Manages the flare’s phases: ignition with fast flickers (50–150 brightness, 20–100ms delays), burning with intense flicker (180–255, ±50) for 2000ms, and burnout with ±30 flicker over 800ms. Automatically transitions to OFF_STATE after burnout.
 * @return 0 on success, per AceRoutine coroutine state definitions.
 */
int SignalFlare::runCoroutine()
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
                setState(IGNITION); // Mark device as busy during transition.
                startTime = millis();
                brightness = 0;
            }

            // Process internal states for ON_STATE.
            switch (getState())
            {
            case IGNITION:
                // Simulate flare ignition with fast flickers (50–150 brightness).
                brightness = random(SIGNALFLARE_IGNITION_MIN_BRIGHTNESS, SIGNALFLARE_IGNITION_MAX_BRIGHTNESS + 1);
                simulatePWM(_pin, brightness, SIGNALFLARE_PWM_PERIOD_US);
                COROUTINE_DELAY(random(20, 101)); // Random delay (20–100ms).
                // Transition to BURNING after 500ms.
                if (millis() - startTime > SIGNALFLARE_IGNITION_DURATION_MS)
                {
                    setState(BURNING);
                    startTime = millis();
                }
                break;

            case BURNING:
                // Simulate intense burning with large flicker (180–255).
                brightness = SIGNALFLARE_BURNING_TARGET_BRIGHTNESS + random(SIGNALFLARE_BURNING_MIN_VARIATION, SIGNALFLARE_BURNING_MAX_VARIATION + 1);
                brightness = constrain(brightness, SIGNALFLARE_BURNING_MIN_BRIGHTNESS, 255);
                simulatePWM(_pin, brightness, SIGNALFLARE_PWM_PERIOD_US);
                // Transition to BURNOUT after 2000ms.
                if (millis() - startTime > SIGNALFLARE_BURNING_DURATION_MS)
                {
                    setState(BURNOUT);
                    startTime = millis();
                }
                break;

            case BURNOUT:
                // Decrease brightness with ±30 flicker over 800ms.
                if (millis() - startTime > SIGNALFLARE_BURNOUT_STEP_MS)
                {
                    brightness -= (255 * SIGNALFLARE_BURNOUT_STEP_MS) / SIGNALFLARE_BURNOUT_DURATION_MS;
                    brightness += random(SIGNALFLARE_BURNOUT_MIN_VARIATION, SIGNALFLARE_BURNOUT_MAX_VARIATION + 1);
                    brightness = constrain(brightness, 0, 255);
                    startTime = millis();
                }
                simulatePWM(_pin, brightness, SIGNALFLARE_PWM_PERIOD_US);
                // Transition to OFF_STATE when brightness falls below threshold.
                if (brightness <= SIGNALFLARE_OFF_THRESHOLD)
                {
                    outputInactive(_pin);
                    newState(OFF_STATE); // Automatically transition to OFF_STATE after burnout.
                }
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