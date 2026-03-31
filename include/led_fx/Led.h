/**
 * @file Led.h
 * @brief Defines the `Led` class for a basic LED on/off effect.
 *
 * Instant on/off with no animation. Useful for simple indicator LEDs or
 * as a debug/test device type.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2026-03-30
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __LED_H__
#define __LED_H__

#include "LedEffect.h"

/**
 * @class Led
 * @brief Basic LED: instant on/off, no animation.
 *
 * Drives a single LED pin directly to HIGH (on) or LOW (off) with no
 * transition effect.  Suitable for indicator LEDs, test devices, or any
 * output that needs a clean binary state.
 *
 * Inherits from LedEffect; pin assignment and DCC address are configured
 * through the standard JSON device definition.
 */
class Led : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    /**
     * @brief Returns the device type name used in JSON config and the web UI.
     * @return F("Led")
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Led");
    }

    /**
     * @brief Coroutine loop: drives the pin HIGH or LOW based on desired state.
     *
     * Waits for a state-change request, then immediately sets the pin active
     * (ON_STATE → HIGH) or inactive (any other state → LOW).
     * No timing, no PWM, no animation.
     *
     * @return 0 (AceRoutine coroutine convention).
     */
    virtual int runCoroutine() override;
};

#endif // __LED_H__
