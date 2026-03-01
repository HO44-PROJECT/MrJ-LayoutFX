/**
 * @file TurnSignal.h
 * @brief Defines the `TurnSignal` class, which implements a fading turn signal effect.
 *
 * This file provides the class definition for a device that simulates a
 * smooth, pulsing turn signal. It is a concrete implementation of the
 * `LedEffect` base class, using a coroutine to manage the fading effect
 * by incrementally adjusting the PWM intensity.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#ifndef __TURNSIGNAL_H__
#define __TURNSIGNAL_H__

#include "LedEffect.h" // Base class for non-perpetual light effects.

// Turn signal effect configuration constants
#define TURN_SIGNAL_PWM_PERIOD_US 10000 ///< PWM period in microseconds for brightness simulation (100 Hz).
#define TURN_SIGNAL_STEP_DELAY_US 5000  ///< Delay between brightness steps in microseconds (controls fade speed).
#define TURN_SIGNAL_STEP_INCREMENT 5    ///< Brightness increment/decrement per step (0-255, 8-bit PWM).
#define TURN_SIGNAL_MAX_INTENSITY 255   ///< Maximum brightness level (0-255, 8-bit PWM).
#define TURN_SIGNAL_MIN_INTENSITY 0     ///< Minimum brightness level (off state, 0).

/**
 * @class TurnSignal
 * @brief Manages a fading and pulsing turn signal light effect.
 *
 * This class extends LedEffect to implement a coroutine-based light effect
 * with smooth fade-in and fade-out behavior, simulating a turn signal.
 */
class TurnSignal : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    /**
     * @brief Returns the device name.
     * @return The string "TurnSignal".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Turn Signal");
    }

    /**
     * @brief Executes the coroutine logic for the turn signal effect.
     *
     * This coroutine handles fade-in and fade-out by adjusting brightness
     * incrementally using a simulated PWM output.
     *
     * @return Coroutine state from AceRoutine (0 for success).
     */
    virtual int runCoroutine() override;

protected:
    uint8_t brightness = 0; ///< Current brightness level (0-255, 8-bit PWM).
    bool increasing = true; ///< Fade direction: true = increasing, false = decreasing.
};

#endif // __TURNSIGNAL_H__
