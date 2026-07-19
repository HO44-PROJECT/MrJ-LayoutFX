/**
 * @file MrJDBBlocSignal.h
 * @brief Defines the `MrJDBBlocSignal` class for simulating a DB block signal.
 *
 * This header file defines a class that simulates a German "Deutsche Bahn" (DB) block signal.
 * It inherits from `CharliePlexingSignal`, which extends `Device`, to manage two output pins
 * and implements a Persistence of Vision (POV) effect for smooth lamp transitions between states.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "CharliePlexingSignal.h"

/**
 * @def MrJDBBlocSignalState_PIN_COUNT
 * @brief Number of output pins used by a `MrJDBBlocSignal` instance.
 */
#define MrJDBBlocSignalState_PIN_COUNT 2

/**
 * @class MrJDBBlocSignal
 * @brief Manages a simulated Deutsche Bahn block signal with a POV effect.
 *
 * This class extends `CharliePlexingSignal` to implement a state machine and coroutine
 * for controlling a two-pin DB block signal. It uses a Persistence of Vision (POV) effect
 * to simulate smooth fading transitions, mimicking the behavior of traditional light bulbs.
 */
class MrJDBBlocSignal : public CharliePlexingSignal<MrJDBBlocSignalState_PIN_COUNT>
{
public:
    using CharliePlexingSignal::CharliePlexingSignal; ///< Inherit base class constructors.

    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("MrJDBBlocSignal");
    }
    virtual uint8_t getStateCount() const override { return 3; } ///< OFF · HP0 · HP1
    inline virtual void switchOn(bool skipDelay = false) override { newState(HP0_STATE, skipDelay); }

    static const STATE_TYPE OFF_STATE = 0; ///< Signal is off (no light).
    static const STATE_TYPE HP0_STATE = 1; ///< Stop signal (red light).
    static const STATE_TYPE HP1_STATE = 2; ///< Proceed at full speed (green light).
    static const STATE_TYPE TEST_STATE = 9; ///< Test state for demonstration purposes.

#ifdef DEMO
    void demo(uint32_t *lastSwitchTime, uint16_t delay = 5000)
    {
        if (!busy() && (millis() - *lastSwitchTime > delay))
        {
            if (getState() == MrJDBBlocSignal::HP0_STATE)
            {
                newState(MrJDBBlocSignal::HP1_STATE);
            }
            else
            {
                newState(MrJDBBlocSignal::HP0_STATE);
            }
            *lastSwitchTime = millis();
        }
    }
#endif

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
    static const PIN_STATE PIN_STATE_OFF[MrJDBBlocSignalState_PIN_COUNT];
    static const PIN_STATE PIN_STATE_HP0[MrJDBBlocSignalState_PIN_COUNT];
    static const PIN_STATE PIN_STATE_HP1[MrJDBBlocSignalState_PIN_COUNT];
};
