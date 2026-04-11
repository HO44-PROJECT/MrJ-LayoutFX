/**
 * @file SerialServoMotorMode.cpp
 * @brief Implements the `SerialServoMotor` class for controlling an LX-16A servo in motor mode.
 *
 * This file contains the implementation of the constructor for the `SerialServoMotor` class, which initializes
 * the serial bus and the LX-16A servo object for continuous rotation (motor mode) using the lx16a-servo library.
 * The class is designed for railway signaling applications, supporting speed transitions managed through coroutines.
 * Positive states (e.g., non-zero speed settings) are interruptible, while negative states (if defined) are
 * non-stable and uninterruptible, adhering to project conventions.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-17
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "servo/SerialServoMotorMode.h"

#ifdef MRJFX_SERIAL_SERVO_ENABLED

  #ifdef MRJFX_LX16A_SERVO_ENABLED
/**
 * @brief Constructs a `SerialServoMotor` instance for controlling an LX-16A servo.
 *
 * Initializes the serial bus and servo object for communication over a serial bus (TX, RX, and optional direction pin).
 * If no existing bus is provided (servoBus is nullptr), a new LX16ABus object is created using the specified TX pin
 * and direction pin (if provided). The serial communication is configured at the baud rate defined by LX16A_BAUD_RATE.
 * The servo is set to motor mode with an initial speed of SERVO_SPEED_STOP (0). The calling program is responsible for
 * ensuring that the RX pin (if required) and the servo power supply are properly configured.
 *
 * @param servoBus Pointer to an existing LX16ABus object (nullptr if tXpin is used to create a new bus).
 * @param tXpin TX pin for serial communication (default: NO_PIN, indicating no pin assigned).
 * @param TXFlagGPIO Direction pin for 3-pin bus configuration (default: NO_PIN, set to -1 if unused).
 * @param servoID ID of the servo for bus communication (range: 0-253, default: LX16A_SERVO_ID).
 */
SerialServoMotor::SerialServoMotor(LX16ABus *servoBus, PIN_ID tXpin, PIN_ID TXFlagGPIO, int servoID) {
  // Check if an existing servo bus is provided; otherwise, allocate a new one
  if (servoBus == nullptr) {
    servoBus = new LX16ABus(); // Create a new LX16ABus object for serial communication
  }
  // Assign the servo bus pointer to the class instance
  this->servoBus = servoBus;

  // Proceed with initialization only if the servo bus is valid
  if (servoBus != nullptr) {
    // Allocate a new LX16AServo object linked to the bus and specified servo ID
    servo = new LX16AServo(this->servoBus, servoID);

    // Configure the serial bus with the TX pin and direction pin (use -1 for direction if NO_PIN is provided)
    servoBus->begin(&Serial, tXpin, TXFlagGPIO == NO_PIN ? -1 : TXFlagGPIO);

    // Clear the serial buffer to prevent stale data from interfering with communication
    Serial.flush();

    // Initialize serial communication at the predefined baud rate (LX16A_BAUD_RATE)
    Serial.begin(LX16A_BAUD_RATE);

    // Set the servo to motor mode with an initial speed of stopped (SERVO_SPEED_STOP)
    stop();
  }
}
  #endif

  #ifdef MRJFX_LOBOT_SERVO_ENABLED
/**
 * @brief Constructs a SerialServoMotor instance for Nano.
 *
 * Initializes the servo using LobotServo on the Serial port (pins 0/1).
 * If tXpin is specified, it’s assumed to be the TX pin (e.g., pin 1). For single-pin
 * mode or direction pin, configure externally or add pinMode here. The servo is set
 * to motor mode with speed 0 (stopped).
 *
 * @param servo Existing LobotServo object (nullptr to create a new one).
 * @param tXpin TX pin (e.g., 1 for Nano’s TX); ignored if servo is provided.
 * @param TXFlagGPIO Direction pin (optional, default NO_PIN).
 * @param servoID Servo ID (0-253, default 1).
 */
SerialServoMotor::SerialServoMotor(LobotServo *servo, PIN_ID tXpin, PIN_ID TXFlagGPIO, int servoID) {
  if (servo == nullptr) {
    // Initialize Serial for Nano (pins 0/1)
    Serial.begin(115200);
    Serial.flush();
    this->servo = new LobotServo(Serial, servoID);
  } else {
    this->servo = servo;
  }

  // Optional: Configure single-pin or direction pin
  if (tXpin != NO_PIN) {
    pinMode(pinId(tXpin), OUTPUT); // Set TX pin for writes
  }
  if (TXFlagGPIO != NO_PIN) {
    pinMode(pinId(TXFlagGPIO), OUTPUT);
    pinWrite(TXFlagGPIO, LOW); // Default state
  }

  // Set to motor mode with speed 0 (stopped)
  stop();
}
  #endif

  #if defined(MRJFX_LOBOT_SERVO_ENABLED) || defined(MRJFX_LX16A_SERVO_ENABLED)
/**
 * @brief Runs the coroutine for the gas lamp effect.
 *
 * Manages ignition, initial flicker, brightening, stable flame, and extinction phases based on desired state.
 * Uses non-blocking delays and random variations for realism with a single PWM and delay call per loop.
 * @return 0 on success (AceRoutine coroutine state).
 */
int SerialServoMotor::runCoroutine() {
  COROUTINE_LOOP() {
    // Wait for a state change (OFF↔RUN) OR a speed change while running.
    COROUTINE_AWAIT(getState() != getTargetState() || speed != lastSpeed);

    lastSpeed = speed;
    if (getTargetState() == OFF_STATE) {
      if (servo != nullptr) servo->motor_mode(SERVO_SPEED_STOP);
      setState(OFF_STATE);
    } else {
      if (servo != nullptr) servo->motor_mode(speed);
      setState(RUN_STATE);
    }
  }

  return 0;
}
  #endif // End of SerialServoMotor implementation

#endif // MRJFX_SERIAL_SERVO_ENABLED