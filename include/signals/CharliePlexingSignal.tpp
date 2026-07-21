/**
 * @file CharliePlexingSignal.tpp
 * @brief Implements template member functions for CharliePlexingSignal.
 *
 * This file defines the logic for Charlieplexing-based signal effects, including
 * persistence of vision (POV) and state transitions for lighting up or turning off
 * the signal. It uses coroutines for asynchronous timing and requires explicit pin
 * initialization via `initPins` by the caller.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-04
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include "CharliePlexingSignal.h"

/**
 * @brief Configures the worker pins for the Charlieplexing signal effect.
 *
 * Activates or deactivates worker pins based on the `on` parameter to implement
 * visual effects like POV. Assumes pins have been initialized via `initPins`.
 *
 * @tparam PinCount Number of output pins managed by the device (1 to PinCount).
 * @param on True to activate worker pins (set to HIGH), false to deactivate them (set to LOW or INPUT).
 */
template <size_t PinCount>
inline void CharliePlexingSignal<PinCount>::PinItWorker(bool on)
{
    // Toggle worker pins based on the `on` flag.
    if (on)
    {
        for (size_t i = 0; i < worker_pin_count; i++)
        {
            // Activate each worker pin to HIGH state.
            this->outputActive(worker_pins[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < worker_pin_count; i++)
        {
            // Deactivate each worker pin to LOW or INPUT state.
            this->outputInactive(worker_pins[i]);
        }
    }
}

#include "CharliePlexingSignal.h"

/**
 * @brief Executes the coroutine for asynchronous state transitions.
 *
 * Manages signal state transitions (lighting up for signal states or turning off for OFF_STATE)
 * using a coroutine. Configures pins based on the desired state, applies POV effects by
 * toggling worker pins, and updates the current state. Requires explicit pin initialization
 * via `initPins` before running.
 *
 * @tparam PinCount Number of output pins managed by the device (1 to PinCount).
 * @return 0 on success, per AceRoutine coroutine state definitions.
 */
template <size_t PinCount>
int CharliePlexingSignal<PinCount>::runCoroutine()
{
    COROUTINE_LOOP()
    {
        // Wait for a change in the desired state (e.g., new signal state or OFF_STATE).
        DEVICE_WAIT_STATE_CHANGE(this->getTargetState());
        DEVICE_APPLY_START_DELAY();

        this->working_state = this->getTargetState();

        switch (this->getTargetState())
        {
        case this->OFF_STATE:
            // Prepare to turn off all worker pins for OFF_STATE.
            _lighting_up = false;
            break;

        // case this->DEMO_STATE:
        //     // For demo state, we can implement a specific effect or simply light up the signal.
        //     this->setState(this->RUN_TRANSIT_STATE);
        //     return 0;
        //     break;

        default:
            // Initialize worker pins for lighting up a signal state.
            if (this->getState() == this->INIT_STATE)
            {
                // Prepare to light up worker pins for the signal state.
                _lighting_up = true;
                initPins(this->getTargetState(), _lighting_up);
            }

            break;
        }

        this->setState(this->RUN_TRANSIT_STATE);

        FADING_EFFECT(_lighting_up, _iterations, _base_delay_micros, _i, _delay);

        // Update the current state to match the target state after transition.
        this->setState(this->getTargetState());
    }

    return 0; // Success.
}

template <size_t PinCount>
bool CharliePlexingSignal<PinCount>::activateNewTarget(bool /*skipDelay*/)
// TODO: revoirle modèle de classe avec une classe SuperSignal intermédiaire
{
    if (!this->busy())
    {
        // Device is not busy, check for desired state change.
        if (this->getTargetState() != this->getDesiredState())
        {
            // Transition through OFF_STATE if currently in a non-OFF state.
            if (this->getTargetState() != this->OFF_STATE)
            {
                // Turn off LEDs before switching to new state.
                this->targetState = this->OFF_STATE;
                this->state = this->INIT_STATE; // Mark as busy for transition.
                return true;
            }
            // Current state is OFF, activate the desired state.

            this->targetState = this->desiredState;
            this->state = this->INIT_STATE;
            return true;
        }
    }
    return false; // No transition needed or device is busy.
}

template <size_t PinCount>
void CharliePlexingSignal<PinCount>::initPins(STATE_TYPE state, bool lighting_up)
{
    worker_pin_count = 0; // Reset worker pin count to 0.

    // Retrieve the desired signal state configuration from derived class.
    const PIN_STATE *desired_state_definition = getPinState(state);

    // Configure all pins based on the desired state (H, L, or INPUT).
    for (uint8_t pin_no = 0; pin_no < this->getPinCount(); pin_no++)
    {
        if (desired_state_definition[pin_no] == SH && !lighting_up)
        {
            if (!lighting_up)
            {
                this->pin_it(this->_pins[pin_no], L);
                worker_pins[worker_pin_count++] = this->_pins[pin_no];
            }
        }
        else if (desired_state_definition[pin_no] == H)
        {
            // Set high-state pins to LOW temporarily and add to worker pins.
            this->pin_it(this->_pins[pin_no], L);
            worker_pins[worker_pin_count++] = this->_pins[pin_no];
        }
        else
        {
            // Apply non-high pin states (LOW or INPUT) as specified.
            this->pin_it(this->_pins[pin_no], desired_state_definition[pin_no]);
        }
    }
}
