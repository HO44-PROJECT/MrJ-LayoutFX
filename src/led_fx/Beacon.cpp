/**
 * @file Beacon.cpp
 * @brief Implements the coroutine logic for a flashing beacon light effect.
 *
 * This file contains the implementation of the `Beacon` class, providing the state
 * transition and timing logic for a beacon light with a double-flash pattern
 * (two short flashes followed by a longer pause). It uses a coroutine to ensure
 * non-blocking operation, allowing concurrent tasks in the main loop.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-04
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#include "led_fx/Beacon.h"

/// @brief Executes the coroutine for the beacon’s double-flash pattern.
/// @return Coroutine state from AceRoutine.
int Beacon::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for transition needed (ie current state not the target state)
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
            }

            // First flash: turn on LED.
            outputActive(_pin);
            COROUTINE_DELAY(BEACON_FLASH_ON_DURATION_1);
            // Turn off LED after first flash.
            outputInactive(_pin);
            COROUTINE_DELAY(BEACON_FLASH_OFF_DURATION_1);

            // Second flash: turn on LED.
            outputActive(_pin);
            COROUTINE_DELAY(BEACON_FLASH_ON_DURATION_2);
            // Turn off LED after second flash.
            outputInactive(_pin);
            COROUTINE_DELAY(BEACON_FLASH_OFF_DURATION_2);

            // Short pause before restarting the cycle.
            COROUTINE_DELAY(BEACON_SHORT_PAUSE);
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