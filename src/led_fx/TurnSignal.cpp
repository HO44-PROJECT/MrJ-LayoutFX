/**
 * @file TurnSignal.cpp
 * @brief Implements a fading and pulsing turn signal effect using a coroutine.
 *
 * This file contains the state transition and timing logic for a turn signal device.
 * It simulates a smooth fading in and out effect by incrementally adjusting the
 * PWM signal intensity, all managed within a non-blocking coroutine.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license AGPL-3.0-or-later
 */

#include "led_fx/TurnSignal.h"

/**
 * @brief Manages the state transitions and fading effect of the turn signal.
 *
 * This coroutine handles the smooth increase and decrease of the LED's
 * brightness for the ON_STATE state, and ensures the light is off for the OFF_STATE state.
 * It is designed to be non-blocking, allowing other tasks to run concurrently.
 *
 * @return The state of the coroutine, as defined by AceRoutine.
 */
int TurnSignal::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a change in the desired state.
        DEVICE_WAIT_STATE_CHANGE(getTargetState());
        DEVICE_APPLY_START_DELAY();

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

                brightness = TURN_SIGNAL_MIN_INTENSITY;
                increasing = true;
            }

            // Adjust brightness level to simulate a fade in/out effect.
            if (increasing)
            {
                brightness += TURN_SIGNAL_STEP_INCREMENT;
                if (brightness >= TURN_SIGNAL_MAX_INTENSITY)
                {
                    brightness = TURN_SIGNAL_MAX_INTENSITY;
                    increasing = false; // Begin fading out.
                }
            }
            else
            {
                brightness -= TURN_SIGNAL_STEP_INCREMENT;
                if (brightness <= TURN_SIGNAL_MIN_INTENSITY)
                {
                    brightness = TURN_SIGNAL_MIN_INTENSITY;
                    increasing = true; // Begin fading in.
                }
            }

            // Apply brightness using raw PWM simulation.
            simulatePWM_raw(_pin, brightness, TURN_SIGNAL_PWM_PERIOD_US);

            // Wait before next brightness update for smooth fading.
            COROUTINE_DELAY_MICROS(TURN_SIGNAL_STEP_DELAY_US);

            break;

        case OFF_STATE:
            // Turn off the LED immediately.
            outputInactive(_pin);
            setState(OFF_STATE);

            break;
        }
    }

    return 0; // Success.
}
