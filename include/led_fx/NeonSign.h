/**
 * @file NeonSign.h
 * @brief Defines the NeonSign class for a vintage neon sign effect.
 *
 * This header file defines a class that simulates a vintage neon sign with flickering startup,
 * stable glow with occasional buzz-like flickers, and stuttering extinction. It inherits from
 * LedEffect for non-perpetual effect management on a single output pin using a coroutine.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-06
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __NEONSIGN_H__
#define __NEONSIGN_H__

#include "LedEffect.h"

// Neon sign effect configuration constants
#define NEONSIGN_PWM_PERIOD_US 10000               ///< PWM period in microseconds for brightness simulation (100 Hz).
#define NEONSIGN_IGNITION_DURATION_MS 800          ///< Duration of ignition phase in milliseconds.
#define NEONSIGN_IGNITION_MIN_BRIGHTNESS 0         ///< Minimum brightness during ignition phase (0-255).
#define NEONSIGN_IGNITION_MAX_BRIGHTNESS 100       ///< Maximum brightness during ignition phase (0-255).
#define NEONSIGN_IGNITION_FLICKER_MIN_DELAY_MS 50  ///< Minimum delay between flickers in ignition phase in milliseconds.
#define NEONSIGN_IGNITION_FLICKER_MAX_DELAY_MS 200 ///< Maximum delay between flickers in ignition phase in milliseconds.
#define NEONSIGN_BRIGHTENING_STEP_MS 10            ///< Interval for brightness increase steps during brightening phase in milliseconds.
#define NEONSIGN_BRIGHTENING_INCREMENT 1           ///< Brightness increment per step during brightening phase, applied every 10 ms (0-255).
#define NEONSIGN_EXTINCTION_STEP_MS 100            ///< Interval for brightness decrease steps during extinction phase in milliseconds.
#define NEONSIGN_EXTINCTION_DURATION_MS 1200       ///< Duration of extinction phase in milliseconds.
#define NEONSIGN_EXTINCTION_SKIP_CHANCE_PERCENT 20 ///< Chance of skipping a brightness decrease step during extinction phase (0-100%).
#define NEONSIGN_STABLE_LIGHT_INTERVAL_MS 10       ///< Interval for flicker updates during stable glow phase in milliseconds.
#define NEONSIGN_BUZZ_DROP_BRIGHTNESS 150          ///< Brightness level for buzz-like drops during stable glow phase (0-255).
#define NEONSIGN_BUZZ_CHANCE_PERCENT 1             ///< Chance of a buzz-like drop during stable glow phase (0-100%).
#define NEONSIGN_MAX_INTENSITY 255                 ///< Maximum brightness level for PWM (0-255, 8-bit).
#define NEONSIGN_TARGET_STABLE_INTENSITY 245       ///< Target brightness in stable glow phase (0-255).

/**
 * @class NeonSign
 * @brief Simulates a vintage neon sign effect with flickering startup, stable glow, and stuttering extinction.
 *
 * This class extends LedEffect to implement a state machine and coroutine for a non-perpetual neon sign effect
 * on a single output pin.
 */
class NeonSign : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    static const STATE_TYPE IGNITION = NEXT_NON_STABLE;        ///< Rapid, irregular flickers during startup.
    static const STATE_TYPE BRIGHTENING = NEXT_NON_STABLE - 1; ///< Smooth increase to maximum brightness.
    static const STATE_TYPE STABLE_GLOW = NEXT_STABLE;         ///< Steady glow with occasional buzz-like drops.

    /**
     * @brief Returns the device name.
     * @return The string "NeonSign".
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Neon Sign");
    }

    /**
     * @brief Executes the coroutine for the neon sign effect.
     * @return Coroutine state from AceRoutine (0 for success).
     */
    virtual int runCoroutine() override;

protected:
    uint32_t startTime = 0;                                                ///< Start time for phase transitions in milliseconds.
    int16_t brightness = 0;                                                ///< Current brightness level (0-255).

private:
    uint32_t timerStart = 0; ///< Timer for coroutine delay management in milliseconds.
};

#endif // __NEONSIGN_H__