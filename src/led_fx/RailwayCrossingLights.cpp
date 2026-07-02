/**
 * @file RailwayCrossingLights.cpp
 * @brief Implementation of the RailwayCrossingLights class for a railway crossing light effect.
 *
 * This file implements the coroutine-based logic for simulating a railway crossing light.
 * The effect includes a startup phase with brief flickers, a flashing phase with alternating
 * ON/OFF periods, and an extinction phase with gradual dimming.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-05
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "led_fx/RailwayCrossingLights.h"

/**
 * @brief Executes the coroutine for the railway crossing light effect.
 *
 * This coroutine manages the railway crossing light effect through multiple phases:
 * - STARTUP: Brief random flickers to simulate power surge or bulb warmup.
 * - FLASHING: Alternating ON/OFF periods with subtle flicker during ON periods.
 *
 * The coroutine uses random brightness variations and non-blocking delays to create
 * a realistic effect. It responds to changes in targetState (ON_STATE or OFF_STATE)
 * and synchronizes the internal state machine accordingly.
 *
 * @return Coroutine state from AceRoutine (0 for success).
 */
int RailwayCrossingLights::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a change in the desired state (ON_STATE or OFF_STATE).
        DEVICE_WAIT_STATE_CHANGE(getTargetState());

        switch (getTargetState())
        {
        case ON_STATE:
            // Handle transition from OFF to ON.
            if (getState() == INIT_STATE)
            {
                // Initialize the output pins.
                if (!handlePinInitFailure())
                    continue;

                // Start flashing immediately (no asymmetric startup phase).
                setState(FLASHING);
                startTime = millis();
                brightness = RAILWAYCROSSLIGHTS_MAX_INTENSITY;
                isFlashOn = true;
            }

            // Process the internal state machine for ON_STATE.
            switch (getState())
            {
            case FLASHING:
                // Alternate between the two pins: one ON while the other is OFF.
                if (isFlashOn)
                {
                    brightness = RAILWAYCROSSLIGHTS_MAX_INTENSITY;
                    if (random(0, 10) < 2)
                    { // 20% chance of flicker
                        brightness += random(RAILWAYCROSSLIGHTS_FLASH_FLICKER_MIN_VARIATION, RAILWAYCROSSLIGHTS_FLASH_FLICKER_MAX_VARIATION);
                        brightness = constrain(brightness, 0, RAILWAYCROSSLIGHTS_MAX_INTENSITY);
                    }
                    simulatePWM(getPin(0), brightness, RAILWAYCROSSLIGHTS_PWM_PERIOD_US);
                    outputInactive(getPin(1));
                    if (millis() - startTime > RAILWAYCROSSLIGHTS_FLASH_ON_MS)
                    {
                        isFlashOn = false;
                        startTime = millis();
                    }
                }
                else
                {
                    brightness = RAILWAYCROSSLIGHTS_MAX_INTENSITY;
                    if (random(0, 10) < 2)
                    { // 20% chance of flicker
                        brightness += random(RAILWAYCROSSLIGHTS_FLASH_FLICKER_MIN_VARIATION, RAILWAYCROSSLIGHTS_FLASH_FLICKER_MAX_VARIATION);
                        brightness = constrain(brightness, 0, RAILWAYCROSSLIGHTS_MAX_INTENSITY);
                    }
                    outputInactive(getPin(0));
                    simulatePWM(getPin(1), brightness, RAILWAYCROSSLIGHTS_PWM_PERIOD_US);
                    if (millis() - startTime > RAILWAYCROSSLIGHTS_FLASH_OFF_MS)
                    {
                        isFlashOn = true;
                        startTime = millis();
                    }
                }

                // No state change here, perpetual mode
                break;
            }
            break;

        case OFF_STATE:
            // Gradually decrease brightness on both pins simultaneously.
            if (getState() == INIT_STATE)
            {
                setState(RUN_TRANSIT_STATE);
                brightness = RAILWAYCROSSLIGHTS_MAX_INTENSITY;
                startTime  = millis();
            }
            // Gradually decrease brightness to zero with subtle flicker.
            if (millis() - startTime > RAILWAYCROSSLIGHTS_EXTINCTION_STEP_MS)
            {
                brightness -= (RAILWAYCROSSLIGHTS_MAX_INTENSITY * RAILWAYCROSSLIGHTS_EXTINCTION_STEP_MS) / RAILWAYCROSSLIGHTS_EXTINCTION_DURATION_MS;
                brightness += random(RAILWAYCROSSLIGHTS_EXTINCTION_FLICKER_MIN_VARIATION, RAILWAYCROSSLIGHTS_EXTINCTION_FLICKER_MAX_VARIATION);
                brightness = constrain(brightness, 0, RAILWAYCROSSLIGHTS_MAX_INTENSITY);
                startTime = millis();
            }
            // Transition to OFF_STATE when brightness reaches zero.
            if (brightness <= 0)
            {
                outputInactive(getPin(0));
                outputInactive(getPin(1));
                setState(OFF_STATE);
            }
            else
            {
                // Drive both pins simultaneously to avoid alternating flicker.
                // NON-BLOCKING software PWM: COROUTINE_DELAY_MICROS yields to the
                // scheduler between phases. A raw delayMicroseconds() here busy-blocks
                // Core 1 for up to a full PWM period (~10 ms) on every pass, starving
                // every other coroutine and shredding the software-PWM timing of all
                // other fades (this was the real cause of backlog #48). `brightness`
                // is a member, so it survives the yields. Mirrors simulatePWM / the
                // FLASHING phase above.
                if (brightness > 0)
                {
                    outputActive(getPin(0));
                    outputActive(getPin(1));
                    COROUTINE_DELAY_MICROS((uint32_t)brightness * RAILWAYCROSSLIGHTS_PWM_PERIOD_US / 255);
                }
                if (brightness < RAILWAYCROSSLIGHTS_MAX_INTENSITY)
                {
                    outputInactive(getPin(0));
                    outputInactive(getPin(1));
                    COROUTINE_DELAY_MICROS(RAILWAYCROSSLIGHTS_PWM_PERIOD_US - (uint32_t)brightness * RAILWAYCROSSLIGHTS_PWM_PERIOD_US / 255);
                }
            }
            break;
        }
    }

    return 0; // Success
}