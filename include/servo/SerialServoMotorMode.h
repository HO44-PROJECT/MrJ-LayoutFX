/**
 * @file SerialServoMotorMode.h
 * @brief Defines the SerialServoMotor class for controlling an LX-16A servo in motor mode.
 *
 * This class inherits from MultiplePinDevice to manage a LewanSoul/Hiwonder LX-16A servo in
 * continuous rotation (motor mode) using the lx16a-servo library. It supports speed control,
 * stopping, starting, and reversing via a serial bus, with coroutine-based state transitions
 * for railway signaling applications. Positive states (e.g., speed settings) are interruptible,
 * while negative states (if defined) are non-stable and uninterruptible, per project conventions.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-17
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __SERIALSERVOMOTORMODE_H__
#define __SERIALSERVOMOTORMODE_H__

#include "devices/MultiplePinDevice.h"
#include "devices/PinState.h"
#ifdef LX16A
#include <lx16a-servo.h>
#endif
#ifdef LOBOT
#include "servo/LobotServo.h"
#endif

#define SERIAL_SERVO_PIN_COUNT 2 ///< Number of output pins used by a SerialServoMotor instance.

/**
 * @typedef SERVO_SPEED
 * @brief Type alias for servo speed in motor mode.
 *
 * Represents the speed of the LX-16A servo in motor mode, ranging from SERVO_SPEED_MIN (-1000, full reverse)
 * to SERVO_SPEED_MAX (+1000, full forward), with SERVO_SPEED_STOP (0) indicating a stop.
 */
typedef int16_t SERVO_SPEED;

/**
 * @class SerialServoMotor
 * @brief Manages an LX-16A servo in motor mode for continuous rotation and speed control.
 *
 * Extends MultiplePinDevice to control an LX-16A servo via a serial bus (TX, RX, direction).
 * Supports setting rotation speed, stopping, starting, and reversing the servo, using a coroutine
 * for asynchronous state transitions in railway signaling applications. Positive states (e.g., speed settings)
 * are interruptible, while negative states (if defined) are non-stable and uninterruptible.
 *
 * @tparam SERIAL_SERVO_PIN_COUNT Number of pins for serial communication (e.g., TX, RX, direction).
 */
class SerialServoMotor : public MultiplePinDevice<SERIAL_SERVO_PIN_COUNT>
{
public:
    static const STATE_TYPE RUN_STATE = NEXT_STABLE;
    static const STATE_TYPE SPEED_CHANGE_STATE = NEXT_STABLE + 1;

#ifdef LX16A

    /**
     * @brief Constructs a SerialServoMotor with a pre-initialized servo bus.
     *
     * Delegates to the main constructor, using default values for TX pin and direction pin.
     *
     * @param servoBus Pointer to an existing LX16ABus object for serial communication.
     * @param servoID Servo ID for bus communication (range: 0-253, default: LX16A_SERVO_ID).
     */
    SerialServoMotor(LX16ABus *servoBus, int servoID)
        : SerialServoMotor(servoBus, NO_PIN, NO_PIN, servoID) {}

    /**
     * @brief Constructs a SerialServoMotor with a TX pin for single-pin mode.
     *
     * Delegates to the main constructor, using a default direction pin (NO_PIN).
     *
     * @param tXpin TX pin for serial communication (default: NO_PIN).
     * @param servoID Servo ID for bus communication (range: 0-253, default: LX16A_SERVO_ID).
     */
    SerialServoMotor(PIN_ID tXpin, int servoID)
        : SerialServoMotor(nullptr, tXpin, NO_PIN, servoID) {}

    /**
     * @brief Constructs a SerialServoMotor with a TX pin and direction pin for 3-pin mode.
     *
     * Delegates to the main constructor, allowing explicit specification of TX and direction pins.
     *
     * @param tXpin TX pin for serial communication (default: NO_PIN).
     * @param TXFlagGPIO Direction pin for 3-pin bus configuration (default: NO_PIN).
     * @param servoID Servo ID for bus communication (range: 0-253, default: LX16A_SERVO_ID).
     */
    SerialServoMotor(PIN_ID tXpin, PIN_ID TXFlagGPIO, int servoID)
        : SerialServoMotor(nullptr, tXpin, TXFlagGPIO, servoID) {}

    /**
     * @brief Constructs a SerialServoMotor with full serial bus configuration.
     *
     * Initializes the servo bus and servo object for communication. If no bus is provided (servoBus is nullptr),
     * a new bus is created using the TX pin and direction pin (if specified). The caller must ensure
     * that the RX pin (if required) and servo power supply are properly configured.
     *
     * @param servoBus Pointer to an existing LX16ABus object (nullptr if using tXpin).
     * @param tXpin TX pin for serial communication (default: NO_PIN).
     * @param TXFlagGPIO Direction pin for 3-pin bus configuration (default: NO_PIN).
     * @param servoID Servo ID for bus communication (range: 0-253, default: LX16A_SERVO_ID).
     */
    SerialServoMotor(LX16ABus *servoBus = nullptr, PIN_ID tXpin = NO_PIN, PIN_ID TXFlagGPIO = NO_PIN, int servoID = LX16A_SERVO_ID);
#endif

#ifdef LOBOT
    SerialServoMotor(PIN_ID tXpin, int servoID)
        : SerialServoMotor(nullptr, tXpin, NO_PIN, servoID) {}
    SerialServoMotor(PIN_ID tXpin, PIN_ID TXFlagGPIO, int servoID)
        : SerialServoMotor(nullptr, tXpin, TXFlagGPIO, servoID) {}
    SerialServoMotor(LobotServo *servo = nullptr, PIN_ID tXpin = NO_PIN, PIN_ID TXFlagGPIO = NO_PIN, int servoID = LX16A_SERVO_ID);
#endif

    /**
     * @brief Destructor for SerialServoMotor.
     *
     * Cleans up dynamically allocated servo and servo bus objects to prevent memory leaks.
     */
    virtual ~SerialServoMotor()
    {
// Free allocated memory for servo and servoBus
#ifdef LX16A
        delete servoBus;
#endif
#ifdef LOBOT

#endif
    }

    /**
     * @brief Executes the coroutine for asynchronous speed transitions.
     *
     * Manages servo speed changes (e.g., setting new speeds or stopping) using a coroutine,
     * ensuring smooth transitions for railway signaling applications. Requires initialization
     * of the servo bus before execution. Currently a placeholder; implement as needed using AceRoutine.
     *
     * @return 0 on success, per AceRoutine coroutine state definitions.
     */
    virtual int runCoroutine();

    /**
     * @brief Retrieves the device name for identification.
     *
     * Returns a constant string identifying the device, used for debugging or logging purposes.
     *
     * @return C-string "Servo" stored in PROGMEM.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("SerialServo");
    }

    /**
     * @brief Sets the DCC speed for the servo in motor mode.
     *
     * Updates the servo speed based on a DCC speed command, delegating to setSpeed to apply
     * the speed within the valid range [SERVO_SPEED_MIN, SERVO_SPEED_MAX]. Used to integrate
     * with DCC control systems for railway applications.
     *
     * @param Speed Target speed from DCC command (mapped to servo range).
     */
    virtual void setDccSpeed(int16_t Speed)
    {
        Speed = max(min(Speed, SERVO_SPEED_MAX), SERVO_SPEED_MIN);

        if (speed != Speed)
        {
            speed = Speed;
            targetState = SPEED_CHANGE_STATE;
            newState(speed == 0 ? OFF_STATE : RUN_STATE);
        }
    }

public:
    /**
     * @brief Sets the servo speed in motor mode.
     *
     * Sets the target speed for the LX-16A servo in motor mode, clamping it to the valid range
     * [SERVO_SPEED_MIN, SERVO_SPEED_MAX]. Updates the device state to RUN_STABLE_STATE if the
     * speed is non-zero, or OFF_STATE if the speed is zero. If the servo is not initialized,
     * the speed is set to 0 and the state is set to OFF_STATE.
     *
     * @param speed Target speed (SERVO_SPEED_MIN for full reverse, SERVO_SPEED_STOP for stop,
     *              SERVO_SPEED_MAX for full forward).
     */
    inline virtual void setSpeed(SERVO_SPEED speed)
    {
        // static const char MSG_SPEED_SET[] PROGMEM = "%S: speed set to %d"; // Store in PROGMEM
        // static const char MSG_STOP[] PROGMEM = "%S: stop"; // Store in PROGMEM
    }

    /**
     * @brief Stops the servo by setting the speed to SERVO_SPEED_STOP (0).
     *
     * Calls setSpeed with a speed of 0 to stop the servo's rotation, updating the state to OFF_STATE.
     */
    inline virtual void stop()
    {
        setSpeed(SERVO_SPEED_STOP); // Stop the servo
    }

    /**
     * @brief Starts the servo at the last known speed.
     *
     * Restores the servo to the previously set speed stored in the speed member variable.
     * If the servo is not initialized, the state is set to OFF_STATE.
     */
    inline virtual void start()
    {
        setSpeed(speed); // Restore the last known speed
    }

    /**
     * @brief Reverses the servo's direction by negating the current speed.
     *
     * Sets the servo speed to the negative of the last known speed, effectively reversing
     * the direction of rotation. If the servo is not initialized, the state is set to OFF_STATE.
     */
    inline virtual void reverse()
    {
        setSpeed(-speed); // Set speed to opposite direction
    }

#ifdef DEMO
    void demo(uint32_t *lastSwitchTime, uint16_t delay = 2000)
    {
        if (!busy() && (millis() - *lastSwitchTime > delay))
        {
            if (getState() == Device::OFF_STATE)
            {
                reverse();
            }
            else
            {
                stop();
            }
            *lastSwitchTime = millis();
        }
    }
#endif

protected:
#ifdef LX16A
    LX16AServo *servo = nullptr;  ///< Pointer to the LX-16A servo object for motor mode control, initialized in constructor.
    LX16ABus *servoBus = nullptr; ///< Pointer to the serial bus for LX-16A communication, initialized in constructor.
#endif
#ifdef LOBOT
    LobotServo *servo = nullptr; ///< Pointer to LobotServo object
#endif
    SERVO_SPEED speed = SERVO_SPEED_STOP; ///< Current speed of the servo, initialized to stopped state.

private:
    uint32_t timerStart;
};

#endif // __SERIALSERVOMOTORMODE_H__