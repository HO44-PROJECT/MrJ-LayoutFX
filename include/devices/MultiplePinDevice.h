/**
 * @file MultiplePinDevice.h
 * @brief Defines a template class for managing devices with multiple output pins.
 *
 * This header file defines the `MultiplePinDevice` template class, which inherits from `Device`
 * to manage a fixed number of output pins. It provides functionality to set and retrieve
 * pin identifiers for devices that require multiple pins, such as signals or multi-LED setups.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include "devices/Device.h"
#include "devices/PinState.h"

/**
 * @class MultiplePinDevice
 * @brief Template class for devices with a fixed number of output pins.
 *
 * This class extends `Device` to manage a fixed array of output pins, specified by the template
 * parameter `PinCount`. It provides methods to set and retrieve pin identifiers, suitable for
 * devices like signals or multi-LED configurations.
 *
 * @tparam PinCount The number of output pins managed by the device.
 */
template <size_t PinCount>
class MultiplePinDevice : public Device
{
public:
    MultiplePinDevice()
    {
        // Serial.println("MultiplePinDevice");
        for (size_t pin_index = 0; pin_index < PinCount; pin_index++) {
            _pins[pin_index] = NO_PIN;
        }
    }

    /**
     * @brief Constructs a `MultiplePinDevice` instance with an array of pin identifiers.
     *
     * Initializes the device by setting the specified pins and configuring them
     * via `setPins`.
     *
     * @param pins Array of `PinCount` pin identifiers to be used by the device.
     */
    MultiplePinDevice(const PIN_ID pins[PinCount])
    {
        setPins(PinCount, pins);
    }

    /**
     * @brief Returns the number of output pins managed by this device.
     *
     * @return size_t The number of pins, as defined by the `PinCount` template parameter.
     */
    virtual size_t getPinCount() const override { return PinCount; }

    /**
     * @brief Returns the pin identifier at the specified index.
     *
     * Retrieves the hardware pin identifier for the given index. Returns `NO_PIN`
     * if the index is out of bounds.
     *
     * @param index The index of the pin to retrieve (0 to `PinCount - 1`).
     * @return PIN_ID The corresponding hardware pin identifier, or `NO_PIN` if invalid.
     */
    virtual PIN_ID getPin(size_t index) const override
    {
        return (index < PinCount) ? _pins[index] : NO_PIN;
    }

    /**
     * @brief Sets the pin identifier at the specified index.
     *
     * Updates the pin identifier at the given index and re-initializes the pins
     * to ensure proper configuration. Ignores invalid indices.
     *
     * @param index The index of the pin to set (0 to `PinCount - 1`).
     * @param pin The pin identifier to assign.
     */
    virtual bool setPin(size_t index, PIN_ID pin) override
    {
        if (index < PinCount)
        {
            _pins[index] = pin;
        }
        return validatePins();
    }

protected:
    PIN_ID _pins[PinCount]; ///< Array of pin identifiers managed by the device.
};
