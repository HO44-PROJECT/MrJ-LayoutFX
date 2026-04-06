/**
 * @file DoubleBeacon.cpp
 * @brief Implements the coroutine logic for a flashing beacon light effect.
 *
 * This file contains the implementation of the `DoubleBeacon` class, providing the state
 * transition and timing logic for a beacon light with a double-flash pattern
 * (two short flashes followed by a longer pause). It uses a coroutine to ensure
 * non-blocking operation, allowing concurrent tasks in the main loop.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/DoubleBeacon.h"

/**
 * @brief Executes the coroutine for the beacon’s flashing sequence.
 *
 * This coroutine manages the double-flash pattern for the ON_STATE and ensures the
 * beacon is off for the OFF_STATE. It uses non-blocking delays to maintain the
 * timing defined by the configuration constants in `DoubleBeacon.h`.
 *
 * @return int Coroutine state from AceRoutine.
 */
int DoubleBeacon::runCoroutine()
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

                _firstPinNext = true;
                setState(RUN_STABLE_STATE);
            }

            // Alternate between pin 0 and pin 1 each cycle.
            // No setState() here to avoid blocking OLED I2C transfers on every cycle.
            working_pin = _firstPinNext ? getPin(0) : getPin(1);
            _firstPinNext = !_firstPinNext;

            // First flash (on).
            outputActive(working_pin);
            COROUTINE_DELAY(BEACON_FLASH_ON_DURATION_1);

            // First pause (off).
            outputInactive(working_pin);
            COROUTINE_DELAY(BEACON_FLASH_OFF_DURATION_1);

            // Second flash (on).
            outputActive(working_pin);
            COROUTINE_DELAY(BEACON_FLASH_ON_DURATION_2);

            // Long pause (off).
            outputInactive(working_pin);
            COROUTINE_DELAY(BEACON_FLASH_OFF_DURATION_2);

            // Short pause before repeating the sequence.
            COROUTINE_DELAY(BEACON_SHORT_PAUSE);

            break;

        case OFF_STATE:
            // Turn off the LED.
            outputInactive(working_pin);
            setState(OFF_STATE);
            break;
        }
    }

    return 0; // Success.
}