/**
 * @file TrafficLight3Phase.cpp
 * @brief Implements the 3-phase traffic light state machine with POV lamp effect.
 *
 * Provides full transition logic for a STOP–GO–CAUTION signal.
 * Uses AceRoutine coroutines to sequence states and apply POV fading
 * for realistic lamp behavior.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @date 2025-08-01
 * @license AGPL-3.0-or-later
 */

#include "traffic/TrafficLight3Phase.h"

// ---------------- PIN STATE ARRAYS ----------------
/**
 * @brief Pin configuration for OFF state (all LEDs off).
 */
const PIN_STATE TrafficLight3Phases::PIN_STATE_OFF[TRAFFICLIGHT_PIN_COUNT] = {L, L, L};

/**
 * @brief Pin configuration for STOP state (red ON, others OFF).
 */
const PIN_STATE TrafficLight3Phases::PIN_STATE_STOP[TRAFFICLIGHT_PIN_COUNT] = {H, L, L};

/**
 * @brief Pin configuration for GO state (green ON, others OFF).
 */
const PIN_STATE TrafficLight3Phases::PIN_STATE_GO[TRAFFICLIGHT_PIN_COUNT] = {L, H, L};

/**
 * @brief Pin configuration for FLASHING state (yellow ON).
 */
const PIN_STATE TrafficLight3Phases::PIN_STATE_FLASHING[TRAFFICLIGHT_PIN_COUNT] = {L, L, H};

/**
 * @brief Pin configuration for CAUTION state (yellow steady ON).
 */
const PIN_STATE TrafficLight3Phases::PIN_STATE_CAUTION[TRAFFICLIGHT_PIN_COUNT] = {L, L, H};

// ---------------- TRANSITION TABLES ----------------
/**
 * @brief Transition table from OFF state to possible next states.
 */
const STATE_TYPE TrafficLight3Phases::OFF_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM = {
    {GO_STATE, GO_STATE},
    {FLASHING_STATE, FLASHING_STATE},
    {STOP_STATE, CAUTION_STATE, TRANSITION_OFF_STATE, STOP_STATE},
};

/**
 * @brief Transition table from GO state to possible next states.
 */
const STATE_TYPE TrafficLight3Phases::GO_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM = {
    {OFF_STATE, OFF_STATE},
    {FLASHING_STATE, FLASHING_STATE},
    {STOP_STATE, CAUTION_STATE, STOP_STATE},
};

/**
 * @brief Transition table from FLASHING state to possible next states.
 */
const STATE_TYPE TrafficLight3Phases::FLASH_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM = {
    {OFF_STATE, OFF_STATE},
    {GO_STATE, TRANSITION_OFF_STATE, GO_STATE},
    {STOP_STATE, CAUTION_STATE, TRANSITION_OFF_STATE, STOP_STATE},
};

/**
 * @brief Transition table from STOP state to possible next states.
 */
const STATE_TYPE TrafficLight3Phases::STOP_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM = {
    {OFF_STATE, OFF_STATE},
    {GO_STATE, TRANSITION_OFF_STATE, GO_STATE},
    {FLASHING_STATE, TRANSITION_OFF_STATE, FLASHING_STATE},
};

/**
 * @brief Prepares internal flags and delays before executing a state.
 *
 * Controls _lighting_up for POV fading and sets the duration of the state.
 * Special handling for non-stable states (FLASHING, CAUTION).
 *
 * @param working_state The state about to be executed.
 * @return true if preparation succeeded, false if state is invalid.
 */
bool TrafficLight3Phases::prepareRun(STATE_TYPE working_state)
{
    switch (working_state)
    {
    case OFF_STATE:
    case TRANSITION_OFF_STATE:
        _lighting_up = false;
        delay = 0;
        break;

    case STOP_STATE:
    case GO_STATE:
        _lighting_up = true;
        delay = TRAFFIC_LIGHT_STABLE_DELAY_MS;
        break;

    case FLASHING_STATE:
        _lighting_up = !_lighting_up; // toggles ON/OFF
        delay = _lighting_up ? TRAFFIC_LIGHT_FLASH_ON_MS : TRAFFIC_LIGHT_FLASH_OFF_MS;
        break;

    case CAUTION_STATE:
        _lighting_up = true;
        delay = TRAFFIC_LIGHT_CAUTION_DURATION_MS;
        break;

    default:
        return false;
    }

    return true;
}

/**
 * @brief Returns the transition table for a given state.
 *
 * Returns nullptr if no transition table exists.
 *
 * @param state Current logical state.
 * @return Pointer to transition table array or nullptr.
 */
const STATE_TYPE (*TrafficLight3Phases::getTransitionTable(STATE_TYPE state))[NUM_TRANSITIONS_TARGETS]
{
    switch (state)
    {
    case OFF_STATE:
        return OFF_transitions;
    case STOP_STATE:
        return STOP_transitions;
    case GO_STATE:
        return GO_transitions;
    case FLASHING_STATE:
        return FLASH_transitions;
    default:
        return nullptr;
    }
}

/**
 * @brief Retrieves the pin configuration for a given logical state.
 *
 * @param state Target state.
 * @return Pointer to the corresponding pin state array.
 */
const PIN_STATE *TrafficLight3Phases::getPinState(STATE_TYPE state) const
{
    switch (state)
    {
    case CAUTION_STATE:
        return PIN_STATE_CAUTION;
    case OFF_STATE:
        return PIN_STATE_OFF;
    case STOP_STATE:
        return PIN_STATE_STOP;
    case GO_STATE:
        return PIN_STATE_GO;
    case FLASHING_STATE:
        return PIN_STATE_FLASHING;
    default:
        return PIN_STATE_OFF;
    }
}

/**
 * @brief Coroutine driving the asynchronous traffic light state machine.
 *
 * Waits for new target states, initializes pins, prepares timing, applies POV fading,
 * and enforces stable/non-stable state rules.
 *
 * @return Always returns 0 as per AceRoutine requirements.
 */
int TrafficLight3Phases::runCoroutine()
{
    COROUTINE_LOOP()
    {
        DEVICE_WAIT_STATE_CHANGE(this->getTargetState());
        DEVICE_APPLY_START_DELAY();

        if (getState() == INIT_STATE)
        {
            if (!handlePinInitFailure())
                continue;

            if (getTargetState() == FLASHING_STATE)
                _lighting_up = false;
        }

        working_state = *transition_index;

        if (working_state < 0)
            setState(working_state);

        delay = 0;

        if (!prepareRun(working_state))
            continue;

        if (working_state != TRANSITION_OFF_STATE)
            initPins(working_state, _lighting_up);

        FADING_EFFECT(_lighting_up, _iterations, _base_delay_micros, _i, _delay);
        COROUTINE_DELAY_MILLIS(timerStart, delay);

        if (working_state == FLASHING_STATE)
        {
            setState(FLASHING_RUN_STATE);
        }
        else
        {
            if (working_state != this->getTargetState())
            {
                setState(working_state);
                ++transition_index;
            }
            else
            {
                setState(working_state); // remain until coroutine ends
            }
        }
    }

    return 0;
}

/**
 * @brief Activates a new target state if allowed by current busy state.
 *
 * Ensures device is not in non-interruptible state before transitioning.
 *
 * @return true if a new target was activated, false otherwise.
 */
bool TrafficLight3Phases::activateNewTarget(bool /*skipDelay*/)
{
    if (!busy() && targetState != desiredState)
    {
        const STATE_TYPE(*table)[NUM_TRANSITIONS_TARGETS] = getTransitionTable(targetState);
        if (!table)
            return false;

        transition_index = {nullptr};

        for (size_t i = 0; i < NUM_TRANSITIONS; ++i)
        {
            transition_index = {&table[i][0]};
            if (transition_index[0] == desiredState)
                break;
        }

        if (!transition_index)
            return false;

        ++transition_index;

        targetState = desiredState;
        state = INIT_STATE;

        return true;
    }
    return false;
}

/**
 * @brief Maps DCC signal output commands to traffic light states.
 *
 * 1 → STOP, 2 → GO, 3 → FLASHING, other → OFF.
 *
 * @param State Input DCC accessory value.
 */
void TrafficLight3Phases::setDccSigOutputState(uint8_t State)
{
    switch (State)
    {
    case TRAFFIC_LIGHT_ASPECT_ID_STOP:
        newState(STOP_STATE);
        break;
    case TRAFFIC_LIGHT_ASPECT_ID_GO:
        newState(GO_STATE);
        break;
    case TRAFFIC_LIGHT_ASPECT_ID_FLASHING:
        newState(FLASHING_STATE);
        break;
    default:
        newState(OFF_STATE);
    }
}

/**
 * @brief Maps DCC accessory state to traffic light.
 *
 * 0 → STOP, non-zero → GO.
 *
 * @param State Input accessory state.
 */
void TrafficLight3Phases::setDccAccessoryState(uint8_t State)
{
    newState(State == 0 ? STOP_STATE : GO_STATE);
}
