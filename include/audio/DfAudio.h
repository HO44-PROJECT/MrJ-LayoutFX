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

#ifndef __DFAUDIO_H__
#define __DFAUDIO_H__

#include "devices/MultiplePinDevice.h"
#include "devices/PinState.h"
#include <SoftwareSerial.h>

#define DFAUDIO_PIN_COUNT 2 ///< Number of output pins used by a Df Audio device.

typedef uint8_t DFAUDIO_VOLUME;

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
class DfAudio : public MultiplePinDevice<DFAUDIO_PIN_COUNT>
{
public:
    using MultiplePinDevice::MultiplePinDevice;

    /**
     * @brief Constructs a SerialServoMotor with a TX pin and direction pin for 3-pin mode.
     *
     * Delegates to the main constructor, allowing explicit specification of TX and direction pins.
     *
     * @param tXpin TX pin for serial communication (default: NO_PIN).
     * @param TXFlagGPIO Direction pin for 3-pin bus configuration (default: NO_PIN).
     * @param servoID Servo ID for bus communication (range: 0-253, default: LX16A_SERVO_ID).
     */
    DfAudio(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed = 9600);

    DfAudio(PIN_ID rxPin, PIN_ID txPin) : DfAudio(nullptr, rxPin, txPin, 9600)
    {
        setPins(DFAUDIO_PIN_COUNT, rxPin, txPin);
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
    virtual int runCoroutine() { return 0; }

    /**
     * @brief Retrieves the device name for identification.
     *
     * Returns a constant string identifying the device, used for debugging or logging purposes.
     *
     * @return C-string "Servo" stored in PROGMEM.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("DfAudio");
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
        // TODO: map?
        setVolume(Speed);
    }

public:
    /**
     * @brief Stops the servo by setting the speed to 0.
     *
     * Calls setSpeed with a speed of 0 to stop the servo's rotation, updating the state to OFF_STATE.
     */
    inline virtual void stop()
    {
        setVolume(0); // Stop the servo
    }

    /**
     * @brief Starts the servo at the last known speed.
     *
     * Restores the servo to the previously set speed stored in the speed member variable.
     * If the servo is not initialized, the state is set to OFF_STATE.
     */
    inline virtual void start()
    {
        setVolume(volume); // Restore the last known speed
    }

public:
    virtual void setDccAccessoryState(uint8_t State)
    {
        if (State == LAMP_ASPECT_ID_ON)
        {
            // Serial.println("play!");
            setState('P');
            setVolume(15);
            // delay(2000);
            playTrack(3);
            // delay(20000);
        }
        else
        {
            stopPlayback();
        }
    }

    bool waitForAck(unsigned long timeoutMs = 1000);

    // Contrôle de lecture
    void playTrack(uint8_t trackNumber);
    void nextTrack();
    void previousTrack();
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();

    // Volume
    void setVolume(uint8_t level);
    void increaseVolume();
    void decreaseVolume();

    // Modes de lecture
    void repeatPlayback(uint8_t trackNumber);
    void randomPlayback();

    // Contrôle par dossier/fichier
    void playSpecificFolder(uint8_t folderNumber, uint8_t fileNumber);
    void compositePlayback(uint8_t fileSequence[], size_t length);

    // Gestion alimentation
    void resetModule();
    void enterLowPowerMode();

    /**
     * @brief Envoie une commande brute au module.
     * @param command Tableau d’octets représentant la commande.
     * @param length Taille du tableau.
     */
    void sendCommand(const uint8_t command[], size_t length);

    SoftwareSerial *serial = nullptr;         ///<
    DFAUDIO_VOLUME volume = 0; ///< Current speed of the servo, initialized to stopped state.
};

#endif // __DFAUDIO_H__