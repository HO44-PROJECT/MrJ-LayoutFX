/**
 * @file MrJDbExitSignal.cpp
 * @brief Implements the state machine for a DB Exitk Signal with a POV lamp effect.
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

#include "signals/MrJDbExitSignal.h"

// Predefined pin state arrays for different signal states.
// These arrays define the HIGH/LOW state for each LED of the signal.
const PIN_STATE MrJDBExitSignal::PIN_STATE_OFF[MrJDBExitSignalState_PIN_COUNT] = {Z, Z, Z, Z};
const PIN_STATE MrJDBExitSignal::PIN_STATE_HP00[MrJDBExitSignalState_PIN_COUNT] = {L, H, H, L};
const PIN_STATE MrJDBExitSignal::PIN_STATE_HP1[MrJDBExitSignalState_PIN_COUNT] = {Z, L, Z, H};
const PIN_STATE MrJDBExitSignal::PIN_STATE_HP2[MrJDBExitSignalState_PIN_COUNT] = {H, L, L, H};
const PIN_STATE MrJDBExitSignal::PIN_STATE_HP0_SH1[MrJDBExitSignalState_PIN_COUNT] = {L, H, Z, H};

/**
 * @brief Sets the LED state based on a DCC accessory command.
 *
 * Turns the LED effect on or off based on the provided state. If State is 0,
 * the effect is turned off; otherwise, it is turned on.
 *
 * @param State Accessory state (0 for off, non-zero for on).
 */
void MrJDBExitSignal::setDccSigOutputState(uint8_t State)
{
    DEBUG_PRINTLN(F("dcc callback for exit signal"));
    DEBUG_PRINTLN(State);

    switch (State)
    {
    case DB_SIGNAL_ASPECT_ID_HP0:
        newState(HP00_STATE);
        break;
    case DB_SIGNAL_ASPECT_ID_HP1:
        newState(HP1_STATE);
        break;
    case DB_SIGNAL_ASPECT_ID_HP2:
        newState(HP2_STATE);
        break;
    case DB_SIGNAL_ASPECT_ID_HP0_SH1:
        newState(HP0_SH1_STATE);
        break;
    default:
        newState(OFF_STATE);
        break;
    }
}

const PIN_STATE *MrJDBExitSignal::getPinState(STATE_TYPE state) const
{
    switch (state)
    {
    case OFF_STATE:
        return PIN_STATE_OFF;
    case HP00_STATE:
        return PIN_STATE_HP00;
    case HP1_STATE:
        return PIN_STATE_HP1;
    case HP2_STATE:
        return PIN_STATE_HP2;
    case HP0_SH1_STATE:
        return PIN_STATE_HP0_SH1;
    default:
        return PIN_STATE_OFF;
    }
}

char MrJDBExitSignal::statusChar()
{
    switch (state)
    {
    case HP00_STATE:
        return 'R';
    case HP1_STATE:
        return 'G';
    case HP2_STATE:
        return 'L';
    case HP0_SH1_STATE:
        return 'M';
    }
    return CharliePlexingSignal::statusChar();
}
