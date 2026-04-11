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

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_SERIAL_SERVO_ENABLED

  #include "devices/MultiplePinDevice.h"
  #include "devices/PinState.h"

  #ifdef MRJFX_LX16A_SERVO_ENABLED
    #include <lx16a-servo.h>
  #endif
  #ifdef MRJFX_LOBOT_SERVO_ENABLED
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
class SerialServoMotor : public MultiplePinDevice<SERIAL_SERVO_PIN_COUNT> {
public:
  static const STATE_TYPE RUN_STATE = NEXT_STABLE;

  #ifdef MRJFX_LX16A_SERVO_ENABLED

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

  #ifdef MRJFX_LOBOT_SERVO_ENABLED
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
  virtual ~SerialServoMotor() {
  // Free allocated memory for servo and servoBus
  #ifdef MRJFX_LX16A_SERVO_ENABLED
    delete servoBus;
  #endif
  #ifdef MRJFX_LOBOT_SERVO_ENABLED

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
  virtual const __FlashStringHelper *getDeviceName() const override {
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
  virtual void setDccSpeed(int16_t Speed) { setSpeed((SERVO_SPEED)Speed); }

public:
  /**
   * @brief Sets the servo speed [-1000…+1000]. Triggers the coroutine.
   *
   * Clamps to [SERVO_SPEED_MIN, SERVO_SPEED_MAX], updates the speed member and
   * calls newState() to wake the coroutine. 0 → OFF_STATE, non-zero → RUN_STATE.
   */
  inline virtual void setSpeed(SERVO_SPEED newSpeed) {
    newSpeed = max(min(newSpeed, SERVO_SPEED_MAX), SERVO_SPEED_MIN);
    speed = newSpeed;  // Coroutine picks it up immediately via COROUTINE_AWAIT / next cycle.
    // Keep desiredState consistent for the UI (/api/devices "desired" field).
    desiredState = (speed == SERVO_SPEED_STOP) ? OFF_STATE : RUN_STATE;
  }

  /**
   * @brief Set motor speed from the web API (speed in [-1000, +1000]).
   *
   * Called by /api/servo endpoint. Delegates to setSpeed().
   */
  inline virtual void setMotorSpeed(int16_t s) override { setSpeed((SERVO_SPEED)s); }
  inline virtual void reverseMotor() override { reverse(); }

  #ifdef MRJFX_LOBOT_SERVO_ENABLED
  /**
   * @brief Hardware health check: confirms the servo responds on the bus.
   * @return 0 = OK, 1 = no response, 2 = bad response, -1 = not initialised.
   */
  inline virtual int healthCheck() override {
    if (servo == nullptr) return -1;
    return servo->healthCheckBlocking();
  }

  /**
   * @brief Append mode and speed read from the servo to the health JSON.
   */
  inline virtual void appendHealthJson(String& json) override {
    if (servo == nullptr) return;
    json += F(",\"hw_mode\":");  json += servo->getHealthMode();
    json += F(",\"hw_speed\":"); json += servo->getHealthSpeed();
  }
  #endif

  /**
   * @brief Stops the servo by setting the speed to SERVO_SPEED_STOP (0).
   *
   * Calls setSpeed with a speed of 0 to stop the servo's rotation, updating the state to OFF_STATE.
   */
  inline virtual void stop() {
    setSpeed(SERVO_SPEED_STOP); // Stop the servo
  }

  /**
   * @brief Starts the servo at the last known speed.
   *
   * Restores the servo to the previously set speed stored in the speed member variable.
   * If the servo is not initialized, the state is set to OFF_STATE.
   */
  inline virtual void start() {
    setSpeed(speed); // Restore the last known speed
  }

  /**
   * @brief Reverses the servo's direction by negating the current speed.
   *
   * Sets the servo speed to the negative of the last known speed, effectively reversing
   * the direction of rotation. If the servo is not initialized, the state is set to OFF_STATE.
   */
  inline virtual void reverse() {
    setSpeed(-speed); // Set speed to opposite direction
  }

  #ifdef DEMO
  void demo(uint32_t *lastSwitchTime, uint16_t delay = 2000) {
    if (!busy() && (millis() - *lastSwitchTime > delay)) {
      if (getState() == Device::OFF_STATE) {
        reverse();
      } else {
        stop();
      }
      *lastSwitchTime = millis();
    }
  }
  #endif

protected:
  #ifdef MRJFX_LX16A_SERVO_ENABLED
  LX16AServo *servo = nullptr;  ///< Pointer to the LX-16A servo object for motor mode control, initialized in constructor.
  LX16ABus *servoBus = nullptr; ///< Pointer to the serial bus for LX-16A communication, initialized in constructor.
  #endif
  #ifdef MRJFX_LOBOT_SERVO_ENABLED
  LobotServo *servo = nullptr; ///< Pointer to LobotServo object
  #endif
  SERVO_SPEED speed     = SERVO_SPEED_STOP; ///< Requested speed (set by setSpeed / API).
  SERVO_SPEED lastSpeed = SERVO_SPEED_STOP; ///< Last speed sent to the servo hardware (coroutine use only).

private:
  uint32_t timerStart;
};

#endif // MRJFX_SERIAL_SERVO_ENABLED
