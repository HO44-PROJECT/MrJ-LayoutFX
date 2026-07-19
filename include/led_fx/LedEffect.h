/**
 * @file LedEffect.h
 * @brief Defines the LedEffect class for simulating a flickering LED effect.
 *
 * This header file defines the LedEffect class, which inherits from Device to control
 * a single output pin and simulate a flickering LED effect, such as a campfire flame.
 * It uses a coroutine to generate random intensity fluctuations for a natural flickering
 * effect. Pin initialization must be explicitly triggered by calling initPins, except
 * in the non-standard setPin(PIN_ID pin) method, which automatically calls initPins.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include <devices/Device.h>
#include <devices/Pov.h>

/**
 * @def LED_EFFECT_PIN_COUNT
 * @brief Number of output pins used by a LedEffect instance.
 *
 * Specifies the constant number of pins managed by the LedEffect class (always 1).
 */
#define LED_EFFECT_PIN_COUNT 1

/**
 * @class LedEffect
 * @brief Controls a flickering LED effect on a single output pin.
 *
 * Extends Device to implement a state machine and coroutine for simulating random
 * intensity fluctuations, resembling a campfire or similar light effect. Supports
 * asynchronous state transitions using the AceRoutine library.
 *
 * @note The setPin(PIN_ID pin) method automatically calls initPins, deviating from
 * the project's convention of explicit pin initialization. Use setPin(size_t index, PIN_ID pin)
 * and call initPins explicitly for consistency.
 */
class LedEffect : public Device
{
public:
    /**
     * @brief Constant for the device's on state.
     */
    static const STATE_TYPE ON_STATE = 1;
    static const STATE_TYPE LED_RUN_STATE = 2; ///< State for continuous flickering operation.

    static const STATE_TYPE NEXT_STABLE = ON_STATE + 1;

    /**
     * @brief Constructs a LedEffect instance for a single output pin.
     *
     * Initializes the device with a specified pin. The caller must call initPins
     * explicitly to configure the pin, unless using setPin(PIN_ID pin), which
     * initializes automatically.
     *
     * @param pin Output pin identifier (default: NO_PIN).
     */
    LedEffect(PIN_ID pin = NO_PIN)
    {
        _pin = pin;
        validatePins();
    }

    // inline void setupPWM(uint32_t period_us = 10000)
    // {
    //     uint32_t freq = 1000000UL / period_us;
    //     analogWriteFrequency(freq); // juste la fréquence
    //     analogWriteResolution(8);   // 8 bits
    //     _pwm = true;
    // }

    /**
     * @brief Returns the number of output pins managed by the device.
     *
     * @return size_t The number of pins (always 1 for this class).
     */
    inline virtual size_t getPinCount() const override
    {
        return LED_EFFECT_PIN_COUNT;
    }

    /**
     * @brief Retrieves the pin identifier at the specified index.
     *
     * Returns the hardware pin identifier for the given index. Only index 0 is valid,
     * as the class manages a single pin.
     *
     * @param index The index of the pin to retrieve (must be 0).
     * @return PIN_ID The pin identifier, or NO_PIN if the index is invalid.
     */
    virtual PIN_ID getPin(size_t index) const override
    {
        return (index < getPinCount()) ? _pin : NO_PIN;
    }

    /**
     * @brief Activates the flickering LED effect.
     *
     * Requests a transition to ON_STATE by calling setDesiredState(ON_STATE) to
     * enable the flickering effect.
     */
    inline virtual void switchOn(bool skipDelay = false)
    {
        // static const char MSG_switchOn[] PROGMEM = "%S: switch on"; // Déclarer en PROGMEM
        // MRJ_DEBUG_PRINTLN(MSG_switchOn, getDeviceName());

        newState(ON_STATE, skipDelay);
    }

    /**
     * @brief Sets the LED state based on a DCC accessory command.
     *
     * Turns the LED effect on or off based on the provided state. If State is 0,
     * the effect is turned off; otherwise, it is turned on.
     *
     * @param State Accessory state (0 for off, non-zero for on).
     */
    virtual void setDccAccessoryState(uint8_t State)
    {
        (State == LAMP_ASPECT_ID_ON) ? switchOn() : switchOff();
    }

    /**
     * @brief Sets the output pin for the device (non-standard interface).
     *
     * Updates the pin identifier and automatically initializes the pin by calling initPins.
     *
     * @param pin The pin identifier to assign.
     * @note This method calls initPins automatically, deviating from the project's
     * convention of explicit pin initialization. Use setPin(size_t index, PIN_ID pin)
     * for consistency.
     */
    inline virtual bool setPin(PIN_ID pin)
    {
        _pin = pin;
        return validatePins();
    }

    /**
     * @brief Sets the output pin for the device at the specified index.
     *
     * Updates the pin identifier if the index is valid (must be 0, as the class manages
     * a single pin). The caller must call initPins explicitly to apply the pin configuration.
     *
     * @param index The index of the pin to set (must be 0).
     * @param pin The pin identifier to assign.
     */
    virtual bool setPin(size_t index, PIN_ID pin) override
    {
        if (index < getPinCount())
            _pin = pin;
        // TODO: vraiment nécessaire?
        return validatePins();
    }

    /**
     * @brief Sets the LED state based on a DCC function command.
     *
     * Turns the LED effect on if the least significant bit of State is 1, otherwise
     * turns it off.
     *
     * @param State Function state bitmask (bit 0: 1 for on, 0 for off).
     */
    virtual void setDccFunction(uint8_t State)
    {
        if (State & 0x01)
            switchOn();
        else
            switchOff();
    }

#ifdef DEMO
    void demo(uint32_t *lastSwitchTime, uint16_t delay = 10000)
    {
        if (!busy() && (millis() - *lastSwitchTime > delay))
        {
            if (getState() == Device::OFF_STATE)
            {
                switchOn();
            }
            else
            {
                switchOff();
            }
            *lastSwitchTime = millis();
        }
    }
#endif

protected:
    PIN_ID _pin; ///< The output pin identifier used for the flickering effect.
    bool _pwm = false;
};
