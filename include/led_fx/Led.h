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
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __LED_H__
#define __LED_H__

#include "LedEffect.h"

/**
 * @class Led
 * @brief Basic LED: instant on/off, no animation.
 */
class Led : public LedEffect
{
public:
    using LedEffect::LedEffect;

    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Led");
    }

    virtual int runCoroutine() override;
};

#endif // __LED_H__
