/**
 * @file CharliePlexingSignal.h
 * @brief Defines a template class for managing Charlieplexing-based signal devices.
 *
 * This class inherits from `MultiplePinDevice` to manage a fixed number of output pins
 * for Charlieplexing-based signal effects, supporting persistence of vision (POV) and
 * state transitions for lighting up or turning off the signal with configurable durations.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "devices/MultiplePinDevice.h"
#include "devices/PinState.h"

// Macro to calculate iterations and base delay, and perform POV effect
// Needs:
//  bool _lighting_up = false;           ///< Flag indicating if the device is in the lighting-up phase.
//  unsigned int _i = 0;                 ///< Counter for coroutine iterations (0 to _iterations-1).
//  unsigned int _delay = 0;             ///< Current delay in microseconds for POV timing.
//  unsigned int _iterations = 0;        ///< Number of iterations for the current POV transition.
//  unsigned int _base_delay_micros = 0; ///< Base delay per POV iteration in microseconds.

#define FADING_EFFECT(_lighting_up, _iterations, _base_delay_micros, _i, _delay)                                                                                                     \
    do                                                                                                                                                                               \
    {                                                                                                                                                                                \
        /* Calculate iterations for POV effect (2000ms for light-up, 300ms for turn-off). */                                                                                         \
        _iterations = round(CHARLIEPLEXING_MS_TO_US * (_lighting_up ? CHARLIEPLEXING_LIGHT_UP_CHANGE_TIME_MS : CHARLIEPLEXING_TURN_OFF_CHANGE_TIME_MS) / CHARLIEPLEXING_POV_MICROS); \
        /* Calculate base delay per iteration for 10000µs POV cycle (100 Hz). */                                                                                                     \
        _base_delay_micros = round(CHARLIEPLEXING_POV_MICROS / _iterations);                                                                                                         \
        for (_i = 0, _delay = 0; _i < _iterations; _i++, _delay += _base_delay_micros)                                                                                               \
        {                                                                                                                                                                            \
            /* Toggle off worker pins for the first part of the POV cycle. */                                                                                                        \
            PinItWorker(!_lighting_up);                                                                                                                                              \
            COROUTINE_DELAY_MICROS(CHARLIEPLEXING_POV_MICROS - _delay);                                                                                                              \
            /* // Toggle on worker pins for the second part of the POV cycle. */                                                                                                     \
            PinItWorker(_lighting_up);                                                                                                                                               \
            /* Delay for the increasing portion of the POV cycle. */                                                                                                                 \
            COROUTINE_DELAY_MICROS(_delay);                                                                                                                                          \
        }                                                                                                                                                                            \
    } while (0)

/**
 * @struct SIGNAL_STATE
 * @brief Defines the pin states for a specific signal state.
 *
 * Holds an array of pin states and a description for a specific signal configuration.
 */
typedef struct
{
    const PIN_STATE *pin_states; ///< Array of PIN_STATEs for each pin (H, L, or INPUT).
    const char *description;     ///< Human-readable description of the signal state.
} SIGNAL_STATE;

/**
 * @class CharliePlexingSignal
 * @brief Template class for Charlieplexing-based signal devices with multiple pins.
 *
 * This class extends `MultiplePinDevice` to control multiple pins using Charlieplexing
 * for visual effects like POV or state transitions. It uses a coroutine for asynchronous
 * transitions and requires explicit pin initialization via `initPins`.
 *
 * @tparam PinCount Number of output pins managed by the device.
 */
template <size_t PinCount>
class CharliePlexingSignal : public MultiplePinDevice<PinCount>
{
public:
    using MultiplePinDevice<PinCount>::MultiplePinDevice; ///< Inherits constructors from `MultiplePinDevice`.

    /**
     * @brief Executes the coroutine for asynchronous state transitions.
     *
     * Manages signal state transitions (lighting up or turning off) with POV effects,
     * updating pin states based on the desired configuration.
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine() override;

protected:
    virtual const PIN_STATE *getPinState(STATE_TYPE state) const = 0;

    /**
     * @brief Activates a new target state for the signal.
     *
     * Checks if the device is not busy and updates the target state to transition
     * to the desired state, turning off LEDs if necessary before switching to a new
     * signal state. Outputs debug information when enabled.
     * @return true if a new target state is activated, false otherwise.
     */
    virtual bool activateNewTarget();

    /**
     * @brief Configures the worker pins for the signal effect.
     *
     * Activates or deactivates worker pins to implement visual effects like POV.
     * @param on True to activate worker pins, false to deactivate them.
     */
    inline virtual void PinItWorker(bool on);

    virtual void initPins(STATE_TYPE state, bool lighting_up);

protected:
    PIN_ID worker_pins[PinCount]; ///< Array of pin identifiers for worker operations (0 to PinCount-1).
    size_t worker_pin_count = 0;  ///< Number of active worker pins (0 to PinCount).

protected:
    bool _lighting_up = false;           ///< Flag indicating if the device is in the lighting-up phase.
    unsigned int _i = 0;                 ///< Counter for coroutine iterations (0 to _iterations-1).
    unsigned int _delay = 0;             ///< Current delay in microseconds for POV timing.
    unsigned int _iterations = 0;        ///< Number of iterations for the current POV transition.
    unsigned int _base_delay_micros = 0; ///< Base delay per POV iteration in microseconds.
    STATE_TYPE working_state;
};

// Include the template function definitions
#include "signals/CharliePlexingSignal.tpp"
