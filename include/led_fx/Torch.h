/**
 * @file Torch.h
 * @brief Defines the `Torch` class for a flickering torch light effect.
 *
 * This header file defines a class that simulates a torch flame with random intensity
 * fluctuations, bright surges, and brief extinction cycles. It inherits from
 * `LedPerpetualEffect` to manage a single output pin and uses a coroutine for
 * non-blocking operation.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-04
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

// Torch effect configuration constants
#define TORCH_PWM_PERIOD_US 50000     ///< PWM period in microseconds (20 Hz).
#define FLICKER_UPDATE_INTERVAL 200   ///< Interval for updating flicker target in milliseconds.
#define FLICKER_SURGE_CHANCE 8        ///< Percentage chance for a bright surge (0-100).
#define FLICKER_EXTINCTION_CHANCE 2   ///< Percentage chance for a rare flame extinction (0-100).
#define EXTINCTION_MIN_COUNT 3        ///< Minimum number of off cycles during an extinction event (1-5).
#define EXTINCTION_MAX_COUNT 5        ///< Maximum number of off cycles during an extinction event (1-5).
#define EXTINCTION_BURST_MS 60        ///< Duration of each off cycle during an extinction event in milliseconds.
#define FLICKER_TARGET_MIN 0.3        ///< Minimum flicker intensity factor (0.0-1.0).
#define FLICKER_TARGET_MAX 1.0        ///< Maximum flicker intensity factor (0.0-1.0).
#define FLICKER_TARGET_BASE 0.6       ///< Base flicker intensity factor (0.0-1.0).
#define FLICKER_TARGET_VARIATION 0.15 ///< Flicker variation range (± value).
#define FLICKER_VARIATION_MIN -15     ///< Minimum value for random flicker variation (hundredths).
#define FLICKER_VARIATION_MAX 16      ///< Maximum value for random flicker variation (hundredths, exclusive).
#define SMOOTHING_FACTOR 0.15         ///< Weight of the new target in the smoothing calculation (0.0-1.0).
#define TORCH_PERCENT_RANGE 100       ///< Range for percentage-based probability calculations (0-100).

/**
 * @class Torch
 * @brief Manages a flickering torch light effect.
 *
 * This class extends `LedPerpetualEffect` to implement a state machine and coroutine
 * for simulating a torch flame with random flickers, surges, and extinction cycles
 * on a single pin.
 */
class Torch : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    /**
     * @brief Returns the device name.
     * @return A constant character pointer to "Torch".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Torch");
    }

    /**
     * @brief Executes the coroutine for the torch flicker effect.
     *
     * Manages state transitions and applies random flickers, surges, and extinction
     * cycles to simulate a torch flame using a coroutine.
     *
     * @return int The coroutine state, as defined by AceRoutine.
     */
    virtual int runCoroutine() override;

protected:
    float currentFlicker = 1.0f;           ///< Current flicker intensity factor (0.0-1.0).
    float targetFlicker = 1.0f;            ///< Target flicker intensity factor (0.0-1.0).
    unsigned long lastFlickerUpdate = 0;   ///< Timestamp of the last flicker target update in milliseconds.
    uint16_t onDuration = 0;               ///< Calculated ON duration for the PWM cycle in microseconds.
    uint16_t offDuration = 0;              ///< Calculated OFF duration for the PWM cycle in microseconds.
    uint8_t extinctionCyclesRemaining = 0; ///< Number of extinction cycles remaining.
    unsigned long waitStartTime = 0;       ///< Timestamp for the start of a wait period in milliseconds.
};
