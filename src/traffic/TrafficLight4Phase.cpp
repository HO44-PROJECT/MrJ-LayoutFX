/**
 * @file TrafficLight4Phase.cpp
 * @brief Implementation of a 4-phase traffic light with POV lamp effect.
 *
 * Extends `TrafficLight3Phases` by introducing the PREPARE state (red + yellow),
 * commonly used in European traffic lights. STOP has a dedicated transition table
 * that routes through PREPARE before switching to GO or FLASHING.
 *
 * PREPARE and CAUTION are non-interruptible states with fixed durations.
 * Other states reuse the base 3-phase implementation.
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license MIT License
 */

#include "traffic/TrafficLight4Phase.h"

/**
 * @brief Pin configuration for PREPARE state (red + yellow ON, green OFF).
 */
const PIN_STATE TrafficLight4Phases::PIN_STATE_PREPARE[TRAFFICLIGHT_PIN_COUNT] = {H, L, H};

/**
 * @brief Transition table for STOP state.
 *
 * Provides a custom path through PREPARE before reaching GO or FLASHING.
 * Stored in program memory to save RAM.
 */
const STATE_TYPE TrafficLight4Phases::STOP_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM = {
    {OFF_STATE, OFF_STATE},
    {GO_STATE, PREPARE_STATE, TRANSITION_OFF_STATE, GO_STATE},
    {FLASHING_STATE, PREPARE_STATE, TRANSITION_OFF_STATE, FLASHING_STATE},
};

/**
 * @brief Prepare execution for the given working state.
 *
 * Handles PREPARE locally (fixed delay, non-interruptible).
 * All other states are delegated to the base class.
 *
 * @param working_state Current state being executed.
 * @return true if successfully prepared, false if invalid.
 */
bool TrafficLight4Phases::prepareRun(STATE_TYPE working_state)
{
    if (working_state == PREPARE_STATE)
    {
        _lighting_up = true;
        delay = PREPARE_DURATION_MS;
        return true;
    }
    return TrafficLight3Phases::prepareRun(working_state);
}

/**
 * @brief Select the transition table for the current state.
 *
 * STOP uses its own table; other states defer to base class.
 *
 * @param state Current state.
 * @return Pointer to the transition table, or nullptr if invalid.
 */
const STATE_TYPE (*TrafficLight4Phases::getTransitionTable(STATE_TYPE state))[NUM_TRANSITIONS_TARGETS]
{
    if (state == STOP_STATE)
        return STOP_transitions;
    return TrafficLight3Phases::getTransitionTable(state);
}

/**
 * @brief Retrieve pin configuration for the given state.
 *
 * PREPARE handled locally; all other states use base class logic.
 *
 * @param state Target state.
 * @return Pointer to the corresponding pin configuration.
 */
const PIN_STATE *TrafficLight4Phases::getPinState(STATE_TYPE state) const
{
    if (state == PREPARE_STATE)
        return PIN_STATE_PREPARE;
    return TrafficLight3Phases::getPinState(state);
}
