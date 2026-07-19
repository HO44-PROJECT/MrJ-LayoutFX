/**
 * @file DfAudio.cpp
 * @brief Implements the DfAudio class for controlling a DFPlayer Mini (or compatible) audio module.
 *
 * Initializes the SoftwareSerial link to the module and implements the command protocol
 * (7-byte frames: 0x7E <cmd> <ack> <len> <param_hi> <param_lo> 0xEF) for playback control,
 * volume adjustment, playback modes, and power management in railway sound effect applications.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-17
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#include "audio/DfAudio.h"

#ifdef LFX_AUDIO_ENABLED

/**
 * @brief Constructs a DfAudio instance and initializes the SoftwareSerial link.
 *
 * If no existing SoftwareSerial is provided (serial is nullptr), a new one is created on
 * rxPin/txPin and started at the given baud rate.
 *
 * @param serial  Pointer to an existing SoftwareSerial, or nullptr to allocate a new one.
 * @param rxPin   RX pin (connected to module TX).
 * @param txPin   TX pin (connected to module RX).
 * @param speed   Baud rate (default: 9600).
 */
DfAudio::DfAudio(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed)
{
    // Check if an existing servo bus is provided; otherwise, allocate a new one
    if (serial == nullptr)
    {
        // MRJ_DEBUG_PRINTLN("New SoftwareSerial");
        // MRJ_DEBUG_PRINTLN(rxPin);
        // MRJ_DEBUG_PRINTLN(txPin);
        serial = new SoftwareSerial(pinId(rxPin), pinId(txPin));
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

    if ((length > 3 && command[2] == 0x01))
    {
        waitForAck();
    }
}

bool DfAudio::waitForAck(unsigned long timeout)
{
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
        if (serial->available())
        {
            int b = serial->read();
            if (b == 0x7E)
            {
                // début de trame, on pourrait lire le reste
                // pour l’instant on se contente d’un ACK simple
                while (serial->available())
                    serial->read(); // vider le buffer
                // MRJ_DEBUG_PRINTLN("ack ok");
                return true;
            }
        }
    }
    // MRJ_DEBUG_PRINTLN("ack ko");
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
        // MRJ_DEBUG_PRINTLN("Volume level must be between 0 and 30.");
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

#endif // LFX_AUDIO_ENABLED