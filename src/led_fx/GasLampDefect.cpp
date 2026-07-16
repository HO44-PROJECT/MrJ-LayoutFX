/**
 * @file GasLampDefect.cpp
 * @brief Implements the GasLampDefect class for a gas lamp effect with rare malfunctions.
 *
 * Same ignition, unstable flicker, brightening, stable flame, and extinction phases as
 * GasLamp, plus a rare, brief dark glitch reached from the stable flame phase.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/GasLampDefect.h"

static uint8_t extinctionStep(uint8_t bright)
{
    long v = (long)bright
        - (GASLAMP_MAX_INTENSITY * GASLAMP_EXTINCTION_STEP_MS) / GASLAMP_EXTINCTION_DURATION_MS
        + random(GASLAMP_EXTINCTION_FLICKER_MIN_VARIATION, GASLAMP_EXTINCTION_FLICKER_MAX_VARIATION);
    if (v < 0) return 0;
    if (v > bright) return bright;
    return (uint8_t)v;
}

/**
 * @brief Runs the coroutine for the defective gas lamp effect.
 *
 * Manages ignition, initial flicker, brightening, stable flame, rare malfunction,
 * and extinction phases based on desired state. Uses non-blocking delays and random
 * variations, with a single PWM and delay call per loop.
 * @return 0 on success (AceRoutine coroutine state).
 */
int GasLampDefect::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a state change (ON_STATE or OFF_STATE).
        DEVICE_WAIT_STATE_CHANGE(getTargetState());
        DEVICE_APPLY_START_DELAY();

        delayMs = 0; // Delay for COROUTINE_DELAY
        currentTime = millis();

        switch (getTargetState())
        {
        case ON_STATE:
            // Handle transition from OFF to ON.
            if (getState() == INIT_STATE)
            {
                if (!handlePinInitFailure())
                    continue;
                setState(IGNITION);
                startTime = currentTime;
                brightness = 0;
            }

            // Process ON state phases.
            switch (getState())
            {
            case IGNITION:
                // Simulate small irregular flickers during startup.
                brightness = random(GASLAMP_IGNITION_MIN_BRIGHTNESS, GASLAMP_IGNITION_MAX_BRIGHTNESS);
                delayMs = random(GASLAMP_IGNITION_FLICKER_MIN_DELAY_MS, GASLAMP_IGNITION_FLICKER_MAX_DELAY_MS);
                if (currentTime - startTime >= GASLAMP_IGNITION_DURATION_MS)
                {
                    setState(INITIAL_FLICKER);
                    startTime = currentTime;
                }
                break;

            case INITIAL_FLICKER:
                // Simulate larger, unstable flickers.
                brightness = random(GASLAMP_INITIAL_FLICKER_MIN_BRIGHTNESS, GASLAMP_INITIAL_FLICKER_MAX_BRIGHTNESS);
                delayMs = random(GASLAMP_INITIAL_FLICKER_MIN_DELAY_MS, GASLAMP_INITIAL_FLICKER_MAX_DELAY_MS);
                if (currentTime - startTime >= GASLAMP_INITIAL_FLICKER_DURATION_MS)
                {
                    setState(BRIGHTENING);
                    startTime = currentTime;
                    brightness = 0;
                    delayMs = random(GASLAMP_INITIAL_FLICKER_TRANSITION_MIN_DELAY_MS, GASLAMP_INITIAL_FLICKER_TRANSITION_MAX_DELAY_MS);
                }
                break;

            case BRIGHTENING:
                // Gradually increase brightness with variable increments.
                if (currentTime - startTime >= GASLAMP_BRIGHTENING_STEP_MS)
                {
                    if (brightness < GASLAMP_BRIGHTENING_RANGE_1_MAX)
                    {
                        brightness += GASLAMP_BRIGHTENING_INCREMENT_1;
                    }
                    else if (brightness < GASLAMP_BRIGHTENING_RANGE_2_MAX)
                    {
                        brightness += GASLAMP_BRIGHTENING_INCREMENT_2;
                    }
                    else
                    {
                        brightness += GASLAMP_BRIGHTENING_INCREMENT_3;
                    }
                    startTime = currentTime;
                }
                if (brightness >= GASLAMP_MAX_INTENSITY)
                {
                    setState(STABLE_FLAME);
                    startTime = currentTime;
                }
                break;

            case STABLE_FLAME:
                if (currentTime - startTime >= flickerInterval)
                {
                    // Rare roll for a brief malfunction instead of the normal subtle flicker.
                    if (random(GASLAMPDEFECT_MALFUNCTION_CHANCE_RANGE) < GASLAMPDEFECT_MALFUNCTION_CHANCE)
                    {
                        preMalfunctionBrightness = constrain(GASLAMP_TARGET_STABLE_INTENSITY, GASLAMP_STABLE_MIN_INTENSITY, GASLAMP_MAX_INTENSITY);
                        malfunctionDurationMs = random(GASLAMPDEFECT_MALFUNCTION_MIN_MS, GASLAMPDEFECT_MALFUNCTION_MAX_MS);
                        setState(MALFUNCTION);
                        startTime = currentTime;
                        break;
                    }
                    brightness = GASLAMP_TARGET_STABLE_INTENSITY + random(GASLAMP_STABLE_FLICKER_MIN_VARIATION_SUBTLE, GASLAMP_STABLE_FLICKER_MAX_VARIATION_SUBTLE);
                    brightness = constrain(brightness, GASLAMP_STABLE_MIN_INTENSITY, GASLAMP_MAX_INTENSITY);
                    flickerInterval = random(GASLAMP_STABLE_FLICKER_MIN_INTERVAL_MS, GASLAMP_STABLE_FLICKER_MAX_INTERVAL_MS);
                    startTime = currentTime;
                }
                break;

            case MALFUNCTION:
                // Brief dark glitch, then resume normal stable-flame flicker.
                brightness = random(GASLAMPDEFECT_MALFUNCTION_MIN_BRIGHTNESS, GASLAMPDEFECT_MALFUNCTION_MAX_BRIGHTNESS);
                if (currentTime - startTime >= malfunctionDurationMs)
                {
                    setState(STABLE_FLAME);
                    brightness = preMalfunctionBrightness;
                    flickerInterval = random(GASLAMP_STABLE_FLICKER_MIN_INTERVAL_MS, GASLAMP_STABLE_FLICKER_MAX_INTERVAL_MS);
                    startTime = currentTime;
                }
                break;
            }
            break;

        case OFF_STATE:
            // Handle transition to OFF.
            if (getState() == INIT_STATE)
            {
                setState(RUN_TRANSIT_STATE);
                startTime = currentTime;
            }

            if (currentTime - startTime >= GASLAMP_EXTINCTION_STEP_MS)
            {
                brightness = extinctionStep(brightness);
                startTime = currentTime;
            }
            if (brightness <= GASLAMP_BRIGHTENING_OFF_THRESHOLD)
            {
                outputInactive(_pin);
                setState(OFF_STATE);
                delayMs = GASLAMP_STABLE_DELAY_MS;
            }
            break;
        }

        // Single PWM and delay call per loop iteration.
        // simulatePWM routes via pinWrite: GPIO → digitalWrite, SPI → Spi595Bus.
        if (_pwm) {
            analogWrite(pinId(_pin), brightness);
        } else {
            simulatePWM(_pin, brightness, GASLAMP_PWM_PERIOD_US);
        }
        if (delayMs > 0)
        {
            COROUTINE_DELAY(delayMs);
        }
    }

    return 0; // Success
}
