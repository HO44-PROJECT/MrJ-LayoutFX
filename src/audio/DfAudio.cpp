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

#include "audio/DfAudio.h"

/**
 * @brief Constructs a `SerialServoMotor` instance for controlling an LX-16A servo.
 *
 * Initializes the serial bus and servo object for communication over a serial bus (TX, RX, and optional direction pin).
 * If no existing bus is provided (servoBus is nullptr), a new LX16ABus object is created using the specified TX pin
 * and direction pin (if provided). The serial communication is configured at the baud rate defined by LX16A_BAUD_RATE.
 * The servo is set to motor mode with an initial speed of 0. The calling program is responsible for
 * ensuring that the RX pin (if required) and the servo power supply are properly configured.
 *
 * @param servoBus Pointer to an existing LX16ABus object (nullptr if tXpin is used to create a new bus).
 * @param tXpin TX pin for serial communication (default: NO_PIN, indicating no pin assigned).
 * @param TXFlagGPIO Direction pin for 3-pin bus configuration (default: NO_PIN, set to -1 if unused).
 * @param servoID ID of the servo for bus communication (range: 0-253, default: LX16A_SERVO_ID).
 */
DfAudio::DfAudio(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed)
{
    // Check if an existing servo bus is provided; otherwise, allocate a new one
    if (serial == nullptr)
    {
        // Serial.println("New SoftwareSerial");
        // Serial.println(rxPin);
        // Serial.println(txPin);
        serial = new SoftwareSerial(rxPin, txPin);
        serial->begin(speed);
    }
    this->serial = serial;
}

void DfAudio::sendCommand(const uint8_t command[], size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        serial->write(command[i]);
    }

    if ((length > 3 && command[2] == 0x01)) {
        waitForAck();
    }
}

bool DfAudio::waitForAck(unsigned long timeout) {
    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (serial->available()) {
            int b = serial->read();
            if (b == 0x7E) {
                // début de trame, on pourrait lire le reste
                // pour l’instant on se contente d’un ACK simple
                while (serial->available()) serial->read(); // vider le buffer
                // Serial.println("ack ok");
                return true;
            }
        }
    }
    // Serial.println("ack ko");
    return false; // timeout
}

// Contrôle lecture
void DfAudio::playTrack(uint8_t trackNumber)
{
    uint8_t command[] = {0x7E, 0x03, 0x01, 0x02, 0x00, trackNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::nextTrack()
{
    uint8_t command[] = {0x7E, 0x01, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::previousTrack()
{
    uint8_t command[] = {0x7E, 0x02, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::pausePlayback()
{
    uint8_t command[] = {0x7E, 0x0E, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::resumePlayback()
{
    uint8_t command[] = {0x7E, 0x0D, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::stopPlayback()
{
    uint8_t command[] = {0x7E, 0x16, 0x01, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

// Volume
void DfAudio::setVolume(uint8_t level)
{
    if (level > 30)
    {
        // Serial.println("Volume level must be between 0 and 30.");
        return;
    }
    uint8_t command[] = {0x7E, 0x06, 0x01, 0x02, 0x00, level, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::increaseVolume()
{
    uint8_t command[] = {0x7E, 0x04, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::decreaseVolume()
{
    uint8_t command[] = {0x7E, 0x05, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

// Modes de lecture
void DfAudio::repeatPlayback(uint8_t trackNumber)
{
    uint8_t command[] = {0x7E, 0x08, 0x00, 0x02, 0x00, trackNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::randomPlayback()
{
    uint8_t command[] = {0x7E, 0x18, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

// Folders & files
void DfAudio::playSpecificFolder(uint8_t folderNumber, uint8_t fileNumber)
{
    uint8_t command[] = {0x7E, 0x0F, 0x00, 0x02, folderNumber, fileNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::compositePlayback(uint8_t fileSequence[], size_t length)
{
    uint8_t command[5 + length];
    command[0] = 0x7E;
    command[1] = 0x21;
    command[2] = 0x00;
    command[3] = length;
    for (size_t i = 0; i < length; i++)
    {
        command[4 + i] = fileSequence[i];
    }
    command[4 + length] = 0xEF;
    sendCommand(command, sizeof(command));
}

// Power management
void DfAudio::resetModule()
{
    uint8_t command[] = {0x7E, 0x0C, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfAudio::enterLowPowerMode()
{
    uint8_t command[] = {0x7E, 0x0A, 0x00, 0x02, 0x00, 0x01, 0xEF};
    sendCommand(command, sizeof(command));
}
