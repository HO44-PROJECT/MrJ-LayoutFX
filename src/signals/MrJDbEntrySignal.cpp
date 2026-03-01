/**
 * @file MrJDbEntrySignal.cpp
 * @brief Implements the state machine for a DB Entryk Signal with a POV lamp effect.
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

#include "signals/MrJDbEntrySignal.h"

// Predefined pin state arrays for different signal states.
// These arrays define the HIGH/LOW state for each LED of the signal.
const PIN_STATE MrJDBEntrySignal::PIN_STATE_OFF[MrJDBEntrySignalState_PIN_COUNT] = {Z, Z, Z};
const PIN_STATE MrJDBEntrySignal::PIN_STATE_HP0[MrJDBEntrySignalState_PIN_COUNT] = {H, L, L};
const PIN_STATE MrJDBEntrySignal::PIN_STATE_HP1[MrJDBEntrySignalState_PIN_COUNT] = {L, H, L};
const PIN_STATE MrJDBEntrySignal::PIN_STATE_HP2[MrJDBEntrySignalState_PIN_COUNT] = {L, H, H};

/**
 * @brief Sets the LED state based on a DCC accessory command.
 *
 * Turns the LED effect on or off based on the provided state. If State is 0,
 * the effect is turned off; otherwise, it is turned on.
 *
 * @param State Accessory state (0 for off, non-zero for on).
 */
void MrJDBEntrySignal::setDccSigOutputState(uint8_t State)
{
    DEBUG_PRINTLN(F("dcc callback for entry signal"));
    DEBUG_PRINTLN(State);

    switch (State)
    {
    case DB_SIGNAL_ASPECT_ID_HP0:
        newState(HP0_STATE);
        break;
    case DB_SIGNAL_ASPECT_ID_HP1:
        newState(HP1_STATE);
        break;
    case DB_SIGNAL_ASPECT_ID_HP2:
        newState(HP2_STATE);
        break;
    default:
        newState(OFF_STATE);
        break;
    }
}

char MrJDBEntrySignal::statusChar()
{
    switch (state)
    {
    case HP0_STATE:
        return 'R';
    case HP1_STATE:
        return 'G';
    case HP2_STATE:
        return 'L';
    }
    return CharliePlexingSignal::statusChar();
}

const PIN_STATE *MrJDBEntrySignal::getPinState(STATE_TYPE state) const
{
    switch (state)
    {
    case OFF_STATE:
        return PIN_STATE_OFF;
    case HP0_STATE:
        return PIN_STATE_HP0;
    case HP1_STATE:
        return PIN_STATE_HP1;
    case HP2_STATE:
        return PIN_STATE_HP2;
    default:
        return PIN_STATE_OFF;
    }
}
