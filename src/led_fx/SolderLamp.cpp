/**
 * @file SolderLamp.cpp
 * @brief Implements a flickering solder lamp effect using a coroutine.
 *
 * This file contains the state transition and timing logic for a lamp that
 * simulates the intense flashes of an arc welder or a soldering iron. It
 * cycles through states of a dim glow, intense flashes, and pauses, all
 * managed within a non-blocking coroutine.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#include "led_fx/SolderLamp.h"

/**
 * @brief Manages the state transitions and flickering effect of the solder lamp.
 *
 * This coroutine handles the complex light effects for the ON_STATE state,
 * including transitions between a dim glow, flash bursts, and simulated
 * pauses. It is designed to be non-blocking, allowing other tasks to run
 * concurrently.
 *
 * @return The state of the coroutine, as defined by AceRoutine.
 */
int SolderLamp::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait until a state change is requested.
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

                setState(RUN_IDLE);
                totalFlashes = 0;
                currentFlashIndex = 0;
                currentDurationMs = 0;
                offStartTime = 0;
            }

            // Handle the different internal states of the solder lamp effect.
            switch (getState())
            {
            case RUN_IDLE:
                // Random chance to enter an OFF_STATE internal state, simulating a welder pause.
                if (random(100) < OFF_CHANCE_PERCENT)
                {
                    currentDurationMs = random(OFF_DURATION_MIN_MS, OFF_DURATION_MAX_MS);
                    offStartTime = millis();
                    pinWrite(_pin, inactive_state.value);
                    setState(RUN_OFF);
                }
                // Otherwise, randomly start a burst of flashes.
                else if (random(100) < FLASH_CHANCE_PERCENT)
                {
                    totalFlashes = random(FLASH_COUNT_MIN, FLASH_COUNT_MAX + 1);
                    currentFlashIndex = 0;
                    currentDurationMs = random(FLASH_DURATION_MIN_MS, FLASH_DURATION_MAX_MS);
                    setState(RUN_FLASH);
                }
                // Otherwise, simulate a dim, random flickering glow.
                else
                {
                    simulatePWM(_pin, random(GLOW_INTENSITY_MIN, GLOW_INTENSITY_MAX), SOLDERING_PWM_PERIOD_US);
                }
                break;

            case RUN_FLASH:
                // Produce an intense flash at full brightness.
                simulatePWM(_pin, 255, SOLDERING_PWM_PERIOD_US);
                COROUTINE_DELAY_MICROS(currentDurationMs * 1000);

                currentFlashIndex++;
                if (currentFlashIndex < totalFlashes)
                {
                    // If more flashes are needed, pause briefly between flashes in the burst.
                    currentDurationMs = random(FLASH_PAUSE_MIN_MS, FLASH_PAUSE_MAX_MS);
                    setState(RUN_PAUSE);
                }
                else
                {
                    // After the flash burst is complete, return to the idle glow.
                    setState(RUN_IDLE);
                }
                break;

            case RUN_PAUSE:
                // Simulate a dim flickering glow during the pause between flashes.
                simulatePWM(_pin, random(DIM_INTENSITY_MIN, DIM_INTENSITY_MAX), SOLDERING_PWM_PERIOD_US);
                COROUTINE_DELAY_MICROS(currentDurationMs * 1000);

                // Prepare for the next flash duration.
                currentDurationMs = random(FLASH_DURATION_MIN_MS, FLASH_DURATION_MAX_MS);
                setState(RUN_FLASH);
                break;

            case RUN_OFF:
                // The LED is off, so wait for the random OFF_STATE duration to expire.
                COROUTINE_AWAIT(millis() - offStartTime >= currentDurationMs);
                // After the wait, return to the idle state.
                setState(RUN_IDLE);
                break;
            }

            break;

        case OFF_STATE:
            // --- OFF_STATE State: Ensure the lamp is off ---

            // Set the pin to the inactive state to turn off the LED.
            outputInactive(_pin);

            // Update state to OFF_STATE to stop the coroutine loop.
            setState(OFF_STATE);
            break;
        }
    }

    return 0; // Return value for success.
}