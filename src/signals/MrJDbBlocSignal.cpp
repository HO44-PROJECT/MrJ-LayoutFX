/**
 * @file MrJDbBlocSignal.cpp
 * @brief Implements the state machine for a DB Block Signal with a POV lamp effect.
 *
 * This file provides the state transition logic for a multi-state signal device.
 * It uses a coroutine to manage asynchronous state changes and applies a
 * "Persistence of Vision" (POV) effect for smooth transitions between states,
 * emulating a realistic lamp-fading effect.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#include "signals/MrJDbBlocSignal.h"

// Predefined pin state arrays for different signal states.
// These arrays define the HIGH/LOW state for each LED of the signal.
const PIN_STATE MrJDBBlocSignal::PIN_STATE_OFF[MrJDBBlocSignalState_PIN_COUNT] = {L, L};
const PIN_STATE MrJDBBlocSignal::PIN_STATE_HP0[MrJDBBlocSignalState_PIN_COUNT] = {H, L};
const PIN_STATE MrJDBBlocSignal::PIN_STATE_HP1[MrJDBBlocSignalState_PIN_COUNT] = {L, H};

/**
 * @brief Sets the LED state based on a DCC accessory command.
 *
 * Turns the LED effect on or off based on the provided state. If State is 0,
 * the effect is turned off; otherwise, it is turned on.
 *
 * @param State Accessory state (0 for off, non-zero for on).
 */
void MrJDBBlocSignal::setDccSigOutputState(uint8_t State)
{
    MRJ_DEBUG_PRINTLN(F("dcc callback for bloc signal"));
    MRJ_DEBUG_PRINTLN(State);

    newState(State == DB_SIGNAL_ASPECT_ID_HP0 ? HP0_STATE : State == DB_SIGNAL_ASPECT_ID_HP1 ? HP1_STATE
                                                                                             : OFF_STATE);
}

const PIN_STATE *MrJDBBlocSignal::getPinState(STATE_TYPE state) const
{
    switch (state)
    {
    case OFF_STATE:
        return PIN_STATE_OFF;
    case HP0_STATE:
        return PIN_STATE_HP0;
    case HP1_STATE:
        return PIN_STATE_HP1;
    default:
        return PIN_STATE_OFF;
    }
}

char MrJDBBlocSignal::statusChar()
{
    switch (state)
    {
    case HP0_STATE:
        return 'R';
    case HP1_STATE:
        return 'G';
    }
    return CharliePlexingSignal::statusChar();
}
