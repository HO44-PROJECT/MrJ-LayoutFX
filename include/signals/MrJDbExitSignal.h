/**
 * @file MrJDBExitSignal.h
 * @brief Defines the `MrJDBExitSignal` class for simulating a DB exit signal.
 *
 * This header file defines a class that simulates a German "Deutsche Bahn" (DB) exit signal.
 * It inherits from `CharliePlexingSignal`, which extends `Device`, to manage two output pins
 * and implements a Persistence of Vision (POV) effect for smooth lamp transitions between states.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "signals/CharliePlexingSignal.h"

/**
 * @def MrJDBExitSignalState_PIN_COUNT
 * @brief Number of output pins used by a `MrJDBExitSignal` instance.
 */
#define MrJDBExitSignalState_PIN_COUNT 4

/**
 * @class MrJDBExitSignal
 * @brief Manages a simulated Deutsche Bahn exit signal with a POV effect.
 *
 * This class extends `CharliePlexingSignal` to implement a state machine and coroutine
 * for controlling a two-pin DB exit signal. It uses a Persistence of Vision (POV) effect
 * to simulate smooth fading transitions, mimicking the behavior of traditional light bulbs.
 */
class MrJDBExitSignal : public CharliePlexingSignal<MrJDBExitSignalState_PIN_COUNT>
{
public:
    using CharliePlexingSignal::CharliePlexingSignal; ///< Inherit base class constructors.

    virtual const __FlashStringHelper *getDeviceName() const override { return F("MrJDBExitSignal"); }
    virtual uint8_t getStateCount() const override { return 5; } ///< OFF · HP00 · HP1 · HP2 · HP0_SH1
    inline virtual void switchOn(bool skipDelay = false) override { newState(HP00_STATE, skipDelay); }

    static const STATE_TYPE OFF_STATE = 0;  ///< Signal is off (no light).
    static const STATE_TYPE HP00_STATE = 1; ///< Stop signal (red light).
    static const STATE_TYPE HP1_STATE = 2;  ///< Proceed at full speed (green light).
    static const STATE_TYPE HP2_STATE = 3;
    static const STATE_TYPE HP0_SH1_STATE = 4;

protected:
    virtual const PIN_STATE *getPinState(STATE_TYPE state) const override;

    virtual char statusChar();

    /**
     * @brief Sets the LED state based on a DCC accessory command.
     *
     * Turns the LED effect on or off based on the provided state. If State is 0,
     * the effect is turned off; otherwise, it is turned on.
     *
     * @param State Accessory state (0 for off, non-zero for on).
     */
    virtual void setDccSigOutputState(uint8_t State);

    /// Static arrays defining pin states for each signal state.
    static const PIN_STATE PIN_STATE_OFF[MrJDBExitSignalState_PIN_COUNT];
    static const PIN_STATE PIN_STATE_HP00[MrJDBExitSignalState_PIN_COUNT];
    static const PIN_STATE PIN_STATE_HP1[MrJDBExitSignalState_PIN_COUNT];
    static const PIN_STATE PIN_STATE_HP2[MrJDBExitSignalState_PIN_COUNT];
    static const PIN_STATE PIN_STATE_HP0_SH1[MrJDBExitSignalState_PIN_COUNT];
};
