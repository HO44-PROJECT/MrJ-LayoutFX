/**
 * @file RailwayCrossingLights.h
 * @brief Defines the RailwayCrossingLights class for simulating railway crossing light effects.
 *
 * This class simulates a railway crossing light with startup flickers, alternating flashing, and gradual extinction.
 * It inherits from LedPerpetualEffect to control a single LED pin using a coroutine for non-blocking operation.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-05
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "LedEffect.h"

// Railway crossing light effect configuration constants
#define RAILWAYCROSSLIGHTS_PWM_PERIOD_US 10000                  ///< PWM period in microseconds (100Hz) for brightness control.
#define RAILWAYCROSSLIGHTS_MAX_INTENSITY 255                    ///< Maximum brightness level (0-255, 8-bit PWM).
#define RAILWAYCROSSLIGHTS_STARTUP_DURATION_MS 500              ///< Duration of the startup phase in milliseconds.
#define RAILWAYCROSSLIGHTS_EXTINCTION_DURATION_MS 800           ///< Duration of the extinction phase in milliseconds.
#define RAILWAYCROSSLIGHTS_STARTUP_MIN_BRIGHTNESS 0             ///< Minimum brightness during startup phase (0-255).
#define RAILWAYCROSSLIGHTS_STARTUP_MAX_BRIGHTNESS 50            ///< Maximum brightness during startup phase (0-255).
#define RAILWAYCROSSLIGHTS_FLASH_ON_MS 500                      ///< ON period duration in flashing phase in milliseconds.
#define RAILWAYCROSSLIGHTS_FLASH_OFF_MS 500                     ///< OFF period duration in flashing phase in milliseconds.
#define RAILWAYCROSSLIGHTS_STARTUP_FLICKER_MIN_DELAY_MS 50      ///< Minimum delay between flickers in startup phase in milliseconds.
#define RAILWAYCROSSLIGHTS_STARTUP_FLICKER_MAX_DELAY_MS 150     ///< Maximum delay between flickers in startup phase in milliseconds.
#define RAILWAYCROSSLIGHTS_EXTINCTION_STEP_MS 50                ///< Interval for brightness decrease steps during extinction in milliseconds.
#define RAILWAYCROSSLIGHTS_FLASH_FLICKER_MIN_VARIATION -10      ///< Minimum brightness variation during flashing ON periods.
#define RAILWAYCROSSLIGHTS_FLASH_FLICKER_MAX_VARIATION 10       ///< Maximum brightness variation during flashing ON periods.
#define RAILWAYCROSSLIGHTS_EXTINCTION_FLICKER_MIN_VARIATION -10 ///< Minimum brightness variation during extinction phase.
#define RAILWAYCROSSLIGHTS_EXTINCTION_FLICKER_MAX_VARIATION 10  ///< Maximum brightness variation during extinction phase.

/**
 * @class RailwayCrossingLights
 * @brief Simulates a railway crossing light with startup, flashing, and extinction phases.
 *
 * Extends LedPerpetualEffect to manage a single LED pin using a coroutine-based state machine.
 */
class RailwayCrossingLights : public LedEffect
{
public:
    using LedEffect::LedEffect; ///< Inherit base class constructors.

    static const STATE_TYPE STARTUP = NEXT_NON_STABLE; ///< Initial flickers simulating power surge.
    static const STATE_TYPE FLASHING = NEXT_STABLE; ///< Alternating ON/OFF with subtle brightness variations.

    /**
     * @brief Gets the device name for identification.
     * @return "RailwayCrossingLights" for use in debugging or system logs.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("Railway Crossing Lights");
    }

    /**
     * @brief Runs the coroutine for the railway crossing light effect.
     *
     * Manages startup flickers, alternating ON/OFF flashing, and gradual extinction based on desired state.
     * @return 0 on success (AceRoutine coroutine state).
     */
    virtual int runCoroutine() override;

protected:
    uint32_t startTime = 0;                                                                         ///< Timestamp (ms) for tracking phase transitions.
    int16_t brightness = 0;                                                                         ///< Current LED brightness level (0 to 255).
    bool isFlashOn = false;                                                                         ///< Tracks whether the LED is in ON or OFF state during flashing phase.
};
