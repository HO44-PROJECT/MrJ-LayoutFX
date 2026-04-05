/**
 * @file TrafficLight4Phase.h
 * @brief Defines the `TrafficLight4Phases` class for a 4-phase traffic light with POV effect.
 *
 * Extends the 3-phase signal (STOP, GO, CAUTION) with a 4th phase:
 * - PREPARE (red+yellow before green).
 *
 * This adds more realistic road behavior for European-style traffic lights.
 * The state machine uses coroutines for smooth, interruptible transitions.
 * Stable states (STOP, GO, OFF, FLASHING) can be interrupted anytime.
 * Non-stable states (CAUTION, PREPARE) run until completion.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @date 2025-08-04
 * @license MIT License
 */

#pragma once

#include "traffic/TrafficLight3Phase.h"

/**
 * @class TrafficLight4Phases
 * @brief 4-phase road signal (STOP, CAUTION, PREPARE, GO).
 *
 * Adds PREPARE state (red+yellow) to the 3-phase logic.
 * - Provides a custom transition table for STOP.
 * - Uses POV fading like the base class.
 * - Flashing is defined but left unimplemented.
 */
class TrafficLight4Phases : public TrafficLight3Phases
{
public:
    using TrafficLight3Phases::TrafficLight3Phases;

    static constexpr uint16_t PREPARE_DURATION_MS = 2000; ///< Duration of PREPARE state (red+yellow).
                                                          ///< Warns drivers that green is imminent.

    /**
     * @brief Identifies the device type.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("TrafficLight4ph");
    }

    virtual uint8_t getStateCount() const override { return 4; } ///< OFF · STOP · GO · FLASHING

protected:
    // New state constant (extends FSM from base class)
    static const STATE_TYPE PREPARE_STATE = NEXT_NON_STABLE - 2; ///< PREPARE = red+yellow before GO.
                                                                 ///< Non-interruptible (runs full timer).

    // Custom FSM table for STOP state (others use base tables).
    static const STATE_TYPE STOP_transitions[NUM_TRANSITIONS][NUM_TRANSITIONS_TARGETS] PROGMEM;

    /**
     * @brief Pin configuration for given state (R/Y/G).
     */
    virtual const PIN_STATE *getPinState(STATE_TYPE state) const override;

    /**
     * @brief Returns a single-character status.
     * 'p' for PREPARE, else fallback to base.
     */
    inline virtual char statusChar()
    {
        switch (state)
        {
        case PREPARE_STATE:
            return 'p';
        }
        return TrafficLight3Phases::statusChar();
    }

    /**
     * @brief Prepares timers/logic before entering a state.
     * Used to enforce CAUTION/PREPARE durations.
     */
    virtual bool prepareRun(STATE_TYPE working_state);

    /**
     * @brief Runs the coroutine (delegates fully to base).
     */
    virtual int runCoroutine()
    {
        return TrafficLight3Phases::runCoroutine();
    }

    /**
     * @brief Selects transition table for current state.
     * STOP_STATE → custom table
     * All others → base implementation.
     */
    virtual const STATE_TYPE (*getTransitionTable(STATE_TYPE state))[NUM_TRANSITIONS_TARGETS];

    // Lamp pattern for PREPARE (specific to 4-phase lights).
    static const PIN_STATE PIN_STATE_PREPARE[TRAFFICLIGHT_PIN_COUNT]; ///< Red ON, Yellow ON, Green OFF.

private:
    uint32_t timerStart = 0; ///< Timestamp for enforcing uninterruptible states (CAUTION/PREPARE).
};
