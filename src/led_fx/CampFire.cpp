/**
 * @file CampFire.cpp
 * @brief Implements the coroutine logic for a flickering campfire light effect.
 *
 * This file contains the implementation of the `CampFire` class, providing state
 * transition and timing logic for a campfire light with random intensity fluctuations.
 * It uses a coroutine to ensure non-blocking operation, allowing concurrent tasks
 * in the main loop.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/CampFire.h"

/// @brief Executes the coroutine for the campfire flicker effect.
/// @return Coroutine state from AceRoutine.
int CampFire::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a state change request.
        DEVICE_WAIT_STATE_CHANGE(getTargetState());

        switch (getTargetState())
        {
        case ON_STATE:
            // Initializing pins and handling potential failures.
            if (!handlePinInitFailure())
                continue;
            setState(RUN_STABLE_STATE); // Now state is changing, device is busy.

            // Randomly adjust intensity based on probability threshold.
            if (random(0, 100) >= CAMPFIRE_INTENSITY_CHANGE_PROBABILITY)
            {
                // Adjust intensity by a random step.
                intensity += random(CAMPFIRE_INTENSITY_STEP_MIN, CAMPFIRE_INTENSITY_STEP_MAX);
                intensity = constrain(intensity, CAMPFIRE_MIN_INTENSITY, CAMPFIRE_MAX_INTENSITY);
            }

            // Output PWM signal with current intensity.
            simulatePWM_raw(_pin, (uint8_t)intensity, CAMPFIRE_PWM_PERIOD_US);

            // Pause for a random duration to simulate natural flicker.
            COROUTINE_DELAY(random(CAMPFIRE_UPDATE_DELAY_MIN_MS, CAMPFIRE_UPDATE_DELAY_MAX_MS + 1));
            break;

        case OFF_STATE:
            // Turn off the LED.
            outputInactive(_pin);
            // Set state to OFF.
            setState(OFF_STATE);
            break;
        }
    }

    return 0; // Success.
}