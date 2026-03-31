/**
 * @file DfAudio.h
 * @brief Defines the DfAudio class for controlling a DFPlayer Mini (or compatible) audio module.
 *
 * This class inherits from MultiplePinDevice to manage a DFPlayer Mini audio module via a
 * SoftwareSerial UART link. It supports playback control (play, pause, stop, next, previous),
 * volume control, playback modes (repeat, random, folder/file), and power management, using
 * a simple 7-byte frame protocol (0x7E ... 0xEF) for railway sound effect applications.
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
 * @class DfAudio
 * @brief Manages a DFPlayer Mini (or compatible) audio module via SoftwareSerial.
 *
 * Extends MultiplePinDevice to control a DFPlayer Mini audio module connected on RX/TX pins.
 * Supports playback control, volume adjustment, and playback modes using a coroutine for
 * asynchronous operation in railway sound effect applications.
 */
class DfAudio : public MultiplePinDevice<DFAUDIO_PIN_COUNT>
{
public:
    using MultiplePinDevice::MultiplePinDevice;

    /**
     * @brief Constructs a DfAudio with explicit RX and TX pins, creating a new SoftwareSerial.
     *
     * @param rxPin RX pin connected to the module TX.
     * @param txPin TX pin connected to the module RX.
     * @param speed Baud rate for serial communication (default: 9600).
     */
    DfAudio(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed = 9600);

    DfAudio(PIN_ID rxPin, PIN_ID txPin) : DfAudio(nullptr, rxPin, txPin, 9600)
    {
        setPin(0, rxPin);
        setPin(1, txPin);
    }

    /**
     * @brief Coroutine entry point (placeholder, not used for this device).
     *
     * @return 0 always.
     */
    virtual int runCoroutine() { return 0; }

    /**
     * @brief Returns the device name for identification and logging.
     *
     * @return F("DfAudio") stored in PROGMEM.
     */
    virtual const __FlashStringHelper *getDeviceName() const override
    {
        return F("DfAudio");
    }

    /**
     * @brief Sets the audio volume from a DCC speed command.
     *
     * @param Speed Volume level from DCC command (mapped to [0, 30]).
     */
    virtual void setDccSpeed(int16_t Speed)
    {
        // TODO: map?
        setVolume(Speed);
    }

public:
    /**
     * @brief Mutes the audio module by setting volume to 0.
     */
    inline virtual void stop()
    {
        setVolume(0);
    }

    /**
     * @brief Restores the audio module to the last known volume level.
     */
    inline virtual void start()
    {
        setVolume(volume);
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

    SoftwareSerial *serial = nullptr;    ///< SoftwareSerial link to the DFPlayer Mini module.
    DFAUDIO_VOLUME volume = 0;           ///< Current volume level (0–30).
};

#endif // __DFAUDIO_H__