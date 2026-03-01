/**
 * @file VariablePinDevice.h
 * @brief Abstract base class for devices with a variable number of output pins.
 *
 * This class extends the `Device` base class to manage devices with more
 * than one output pin, such as complex signals or lighting effects. It provides
 * a flexible constructor and methods for handling an array of pins.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-01
 * @license MIT License
 */

#ifndef __VARIABLEPINDEVICE_H__
#define __VARIABLEPINDEVICE_H__

#include "Device.h"

/**
 * @class VariablePinDevice
 * @brief An abstract base class for devices with a variable number of pins.
 *
 * This class is designed to be subclassed by concrete implementations that
 * control devices with more than a single output pin. It handles the
 * management of the pin array, allowing for a flexible hardware setup.
 */
class VariablePinDevice : public Device
{
public:
    VariablePinDevice() : pin_count(0), _pins(nullptr) {}

    /**
     * @brief Constructor with explicit pin assignment.
     *
     * Sets the initial state and assigns an array of hardware output pins.
     * This constructor allocates memory for the pins array, which must be
     * managed (deallocated) by the subclass or through a virtual destructor.
     *
     * @param pin_count The number of pins in the array.
     * @param pins An array of `PIN_ID`s corresponding to the physical outputs.
     * @param initial The desired starting signal state (default: OFF_STATE).
     */
    explicit VariablePinDevice(const size_t pin_count, const PIN_ID pins[])
    {
        this->pin_count = pin_count;
        _pins = (PIN_ID *)malloc(pin_count * sizeof(PIN_ID));

        assert(this->_pins);

        setPins(pin_count, pins);
    }

    /**
     * @brief Returns the number of output pins used by this device.
     *
     * @return The number of pins.
     */
    virtual size_t getPinCount() const { return pin_count; }

    /**
     * @brief Returns the pin at the specified index.
     *
     * @param index Pin index.
     * @return PIN_ID The corresponding hardware pin.
     */
    virtual PIN_ID getPin(size_t index) const { return _pins[index]; }

    virtual bool setPin(size_t index, PIN_ID pin)
    {
        _pins[index] = pin;
        return validatePins();
    }

    /**
     * @brief Destructeur virtuel pour libérer la mémoire allouée dynamiquement.
     */
    virtual ~VariablePinDevice()
    {
        if (_pins != nullptr)
        {
            free(_pins);
            _pins = nullptr; // Bonne pratique
        }
    }

protected:
    size_t pin_count; ///< The number of pins in the `_pins` array.
    PIN_ID *_pins;    ///< A dynamically allocated array of output pins.
};

#endif // __VARIABLEPINDEVICE_H__