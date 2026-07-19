/**
 * @file SolderLamp.h
 * @brief Defines the `SolderLamp` class for an arc welder or soldering iron effect.
 *
 * This file provides the class definition for a device that simulates the
 * intense, random flashes of an arc welder or a soldering iron. It is a
 * concrete implementation of the `Device` base class, using a coroutine
 * to manage complex state transitions and timing.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#pragma once

#include "LedEffect.h"

// --- Configuration Constants for the Solder Lamp Effect ---

// PWM period for simulatePWM in microseconds (10 ms)
#define SOLDERING_PWM_PERIOD_US 10000

// Flash effect configuration constants
#define FLASH_CHANCE_PERCENT 2   ///< 2% chance to start a flash burst each loop.
#define FLASH_COUNT_MIN 1        ///< Minimum number of flashes in a burst.
#define FLASH_COUNT_MAX 3        ///< Maximum number of flashes in a burst.
#define FLASH_DURATION_MIN_MS 10 ///< Minimum flash duration in milliseconds.
#define FLASH_DURATION_MAX_MS 30 ///< Maximum flash duration in milliseconds.
#define FLASH_PAUSE_MIN_MS 5     ///< Minimum pause duration between flashes in a burst.
#define FLASH_PAUSE_MAX_MS 20    ///< Maximum pause duration between flashes in a burst.
#define GLOW_INTENSITY_MIN 2     ///< Minimum glow intensity when idle (0-255).
#define GLOW_INTENSITY_MAX 6     ///< Maximum glow intensity when idle (0-255).
#define DIM_INTENSITY_MIN 5      ///< Minimum dim intensity during pause (0-255).
#define DIM_INTENSITY_MAX 15     ///< Maximum dim intensity during pause (0-255).
#define OFF_CHANCE_PERCENT 2     ///< 4% chance to enter an OFF_STATE state (welder pause) each loop.
#define OFF_DURATION_MIN_MS 1000 ///< Minimum OFF_STATE state duration in milliseconds (1 second).
#define OFF_DURATION_MAX_MS 5000 ///< Maximum OFF_STATE state duration in milliseconds (5 seconds).

/**
 * @class SolderLamp
 * @brief Manages a simulated arc welder or soldering iron light effect.
 *
 * This class extends `Device` to provide a state machine and coroutine
 * for simulating the light of an arc welder, including bursts of intense
 * flashes and periods of dim glowing or complete darkness.
 */
class SolderLamp : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    static const STATE_TYPE RUN_IDLE = NEXT_NON_STABLE;      ///< Normal, subtle glowing state.
    static const STATE_TYPE RUN_FLASH = NEXT_NON_STABLE - 1; ///< Intense flash state, representing an arc.
    static const STATE_TYPE RUN_PAUSE = NEXT_NON_STABLE - 2; ///< Brief pause between flashes in a burst.
    static const STATE_TYPE RUN_OFF = NEXT_STABLE;           ///< LED off state, simulating a pause in work.

    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("SolderLamp");
    }

    /**
     * @brief Executes the coroutine logic for asynchronous transitions.
     *
     * Manages state transitions using AceRoutine coroutines.
     *
     * @return int Coroutine state from AceRoutine.
     */
    virtual int runCoroutine() override;

protected:
    uint8_t totalFlashes = 0;       ///< The total number of flashes in the current burst.
    uint8_t currentFlashIndex = 0;  ///< The index of the current flash in the burst.
    uint16_t currentDurationMs = 0; ///< The duration of the current flash or pause in milliseconds.
    unsigned long offStartTime = 0; ///< The timestamp when the OFF_STATE state started.
};
