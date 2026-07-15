/**
 * @file TrafficLight3Phase.h
 * @brief Defines the `TrafficLight3Phases` class for a 3-phase traffic light with POV effect.
 *
 * This class implements a coroutine-based state machine for a 3-phase road signal:
 * - STOP (red)
 * - GO (green)
 * - CAUTION (steady yellow, transition between red/green)
 *
 * It inherits from `CharliePlexingSignal` to drive 3 pins using a persistence-of-vision
 * (POV) effect for smooth fading and transitions.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "signals/CharliePlexingSignal.h"

// Number of pins required for a 3-phase light (R, G, Y)
constexpr size_t TRAFFICLIGHT_PIN_COUNT = 3;

/**
 * @class TrafficLight3Phases
 * @brief Manages stop/go/caution logic with coroutine-driven POV transitions.
 *
 * Provides:
 * - Coroutine engine to animate smooth lamp transitions
 * - State tables for legal transitions (STOP→GO→CAUTION→STOP…)
 * - Integration points for DCC signal and accessory control
 */
class TrafficLight3Phases : public CharliePlexingSignal<TRAFFICLIGHT_PIN_COUNT>
{
public:
    using CharliePlexingSignal::CharliePlexingSignal;

    /**
     * @brief Identifies the device type.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("TrafficLight3ph");
    }

    // Primary state identifiers
    static const STATE_TYPE STOP_STATE = 1;         ///< Red on
    static const STATE_TYPE GO_STATE = 2;           ///< Green on
    static const STATE_TYPE FLASHING_STATE = 3;     ///< Idle flashing amber
    static const STATE_TYPE FLASHING_RUN_STATE = 4; ///< Transitional flashing amber

    virtual bool activateNewTarget(bool skipDelay = false) override;

    virtual uint8_t getStateCount() const override { return 4; } ///< OFF · STOP · GO · FLASHING

    /**
     * @brief Switches on to the default STOP (red) state.
     */
    inline virtual void switchOn(bool skipDelay = false) override { newState(STOP_STATE, skipDelay); }

    /**
     * @brief Provides a single-char status code for external monitoring/debug.
     * @return 'S','G','o','W','F','t', depending on current state.
     */
    inline virtual char statusChar()
    {
        switch (state)
        {
        case STOP_STATE:
            return 'S';
        case GO_STATE:
            return 'G';
        case FLASHING_STATE:
            return 'W';
        case CAUTION_STATE:
            return 'o';
        case FLASHING_RUN_STATE:
            return 'F';
        case TRANSITION_OFF_STATE:
            return 't';
        }
        return CharliePlexingSignal::statusChar();
    }

    // Interfaces for DCC command station integration
    virtual void setDccSigOutputState(uint8_t State);
    virtual void setDccAccessoryState(uint8_t State);

#ifdef DEMO
    void demo(uint32_t *lastSwitchTime, uint16_t delay = 7000)
    {
        if (!busy() && (millis() - *lastSwitchTime > delay))
        {
            if (getState() == STOP_STATE)
            {
                newState(GO_STATE);
            }
            else
            {
                newState(STOP_STATE);
            }
            *lastSwitchTime = millis();
        }
    }
#endif

protected:
    // Transitional states
    static const STATE_TYPE CAUTION_STATE = NEXT_NON_STABLE;            ///< Yellow steady
    static const STATE_TYPE TRANSITION_OFF_STATE = NEXT_NON_STABLE - 1; ///< Intermediary OFF

    // Transition tables (FSM definitions in PROGMEM)
    static constexpr size_t NUM_TRANSITIONS = 3;         ///< Max transitions per state change
    static constexpr size_t NUM_TRANSITIONS_TARGETS = 4; ///< Possible next states

    static const STATE_TYPE OFF_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM;
    static const STATE_TYPE GO_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM;
    static const STATE_TYPE FLASH_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM;
    static const STATE_TYPE STOP_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM;

    virtual const STATE_TYPE (*getTransitionTable(STATE_TYPE state))[NUM_TRANSITIONS_TARGETS];
    virtual const PIN_STATE *getPinState(STATE_TYPE state) const override;

    virtual bool prepareRun(STATE_TYPE working_state);

    /**
     * @brief Coroutine body handling asynchronous state updates
     *        and POV fading for smooth visual effect.
     */
    virtual int runCoroutine() override;

    // Pin patterns for each signal state
    static const PIN_STATE PIN_STATE_OFF[TRAFFICLIGHT_PIN_COUNT];      ///< All lamps off
    static const PIN_STATE PIN_STATE_STOP[TRAFFICLIGHT_PIN_COUNT];     ///< Red ON
    static const PIN_STATE PIN_STATE_GO[TRAFFICLIGHT_PIN_COUNT];       ///< Green ON
    static const PIN_STATE PIN_STATE_CAUTION[TRAFFICLIGHT_PIN_COUNT];  ///< Yellow ON (steady)
    static const PIN_STATE PIN_STATE_FLASHING[TRAFFICLIGHT_PIN_COUNT]; ///< Yellow ON (blinking)

protected:
    uint16_t delay = 0;                         ///< Coroutine delay timer (ms)
    TransitionPtr transition_index = {nullptr}; ///< Pointer into active transition table
    STATE_TYPE working_state;                   ///< Currently processed state inside coroutine

private:
    uint32_t timerStart = 0; ///< Millisecond timestamp reference for delays
};
