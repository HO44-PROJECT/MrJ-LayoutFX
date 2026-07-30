/**
 * @file DfRobotSerialMP3.h
 * @brief Defines the DfRobotSerialMP3 class, a MultiplePinDevice wrapper around a DFR1173
 *        audio module.
 *
 * This class inherits from MultiplePinDevice to manage a DFR1173 audio module as a device
 * within the DCC/coroutine framework (device naming, DCC accessory/speed integration). The
 * actual serial protocol (playback, volume, modes, power) is implemented by DfR1173Bus,
 * which this class holds and delegates to.
 *
 * Reference: product page https://www.dfrobot.com/product-2862.html (DFR1173), wiki
 * https://wiki.dfrobot.com/dfr1173/docs/18471, official (header-only, no PlatformIO release)
 * driver https://github.com/DFRobot/DFRobot_SerialMP3 — see DfR1173Bus for the protocol
 * implementation derived from it.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-17
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_SERIAL_AUDIO_ENABLED

#include "audio/DfR1173Bus.h"
#include "devices/MultiplePinDevice.h"
#include "devices/PinState.h"
#include <SoftwareSerial.h>

#define DFAUDIO_PIN_COUNT 2 ///< Number of output pins used by a DfRobotSerialMP3 device.

/**
 * @class DfRobotSerialMP3
 * @brief Manages a DFRobot DFR1173 (MP3 Voice Prompter) audio module as a MultiplePinDevice.
 *
 * Extends MultiplePinDevice to integrate a DfR1173Bus (serial protocol layer) into the
 * DCC/coroutine device framework — playback, volume and mode calls are delegated to the
 * bus; this class owns only device naming, DCC accessory/speed/function mapping, and the
 * volume fade-effect engine (runCoroutine()).
 */
class DfRobotSerialMP3 : public MultiplePinDevice<DFAUDIO_PIN_COUNT>
{
public:
    static const uint8_t MAX_STATES = 8;

    /**
     * @brief Where playback begins when an AudioState is triggered.
     */
    enum class AudioStart : uint8_t {
        FILE,   ///< Start at a specific file number (start_num), playing from the root.
        FOLDER, ///< Start at the first file of a specific folder number (start_num).
        FIRST,  ///< Start at the first file on the module (start_num ignored).
    };

    /**
     * @brief What happens when the currently playing file reaches its end.
     */
    enum class AudioOnEnd : uint8_t {
        STOP,   ///< Stop after the current file (subject to duration_ms cutting in earlier).
        REPEAT, ///< Loop the current file (module-native single-track loop).
        NEXT,   ///< Advance to the next file in order, looping the module's whole library.
        RANDOM, ///< Advance to a random file (module-native random playback where possible).
    };

    /**
     * @brief One configurable audio "effect" — what to play and how the volume ramps
     *        around it. Modeled on I2cPwmMotorDevice::MotorState (ramp/hold/ramp-down over
     *        a target quantity), substituting volume (0-30) for PWM microseconds.
     *
     * Unlike a motor's perpetual run (duration_ms == 0 meaning "hold forever"),
     * duration_ms == 0 here means "play once, no auto-stop timer" — a one-shot sound
     * effect has no sensible "run forever" state, so 0 simply skips the auto-stop/
     * un-repeat step rather than holding at volume indefinitely (see design doc §3.2/§7).
     */
    struct AudioState {
        AudioStart start;      ///< Where playback begins.
        uint8_t    start_num;  ///< File number (start==FILE) or folder number (start==FOLDER).
        uint8_t    folder_file_num; ///< File number within the folder (start==FOLDER only; 1-255). Ignored otherwise.
        AudioOnEnd on_end;     ///< What to do when the current file ends.
        uint8_t  volume;       ///< Target volume [0-30] held during the run phase.
        uint32_t duration_ms;  ///< Auto-stop after this many ms (0 = no auto-stop timer).
        uint32_t fade_in_ms;   ///< Ramp from current volume to target at run start (0 = instant).
        uint32_t fade_out_ms;  ///< Ramp to 0 at run end or on OFF (0 = instant).
        char     label[16];    ///< Display name (empty = "State N" fallback).
    };

    using MultiplePinDevice::MultiplePinDevice;

    /**
     * @brief Constructs a DfRobotSerialMP3 with explicit RX and TX pins, creating a new DfR1173Bus.
     *
     * @param rxPin RX pin connected to the module TX.
     * @param txPin TX pin connected to the module RX.
     * @param speed Baud rate for serial communication (default: 9600).
     */
    DfRobotSerialMP3(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed = 9600);

    DfRobotSerialMP3(PIN_ID rxPin, PIN_ID txPin, int speed = 9600) : DfRobotSerialMP3(nullptr, rxPin, txPin, speed)
    {
        setPin(0, rxPin);
        setPin(1, txPin);
    }

    /**
     * @brief Frees the owned DfR1173Bus (and its SoftwareSerial) on destruction — without
     *        this, a config hot-reload leaks the bus and leaves a stale SoftwareSerial
     *        interrupt attached to the RX pin, colliding with the next reload's new bus.
     */
    virtual ~DfRobotSerialMP3() { delete bus; }

    /**
     * @brief Device type name, matching the factory key ("DfRobotSerialMP3") used
     *        everywhere else (device_types.json, config.schema.json, icons.js,
     *        OledDisplay::_drawIcon()). Without this override, Device::getDeviceName()'s
     *        "Unknown" default is what reaches OledDisplay::notify() and the
     *        /api/devices JSON — breaking the OLED icon lookup and the WebUI type match.
     */
    virtual const __FlashStringHelper *getDeviceName() const override { return F("DfRobotSerialMP3"); }

    /**
     * @brief No-op: RX/TX pins are already fully owned and configured by DfR1173Bus's
     *        SoftwareSerial (begin() attaches the RX interrupt). The base Device::initPins()
     *        would otherwise call pinMode(rxPin, OUTPUT) + digitalWrite(rxPin, LOW) on the
     *        RX pin right after SoftwareSerial attached its interrupt there — leaving the
     *        interrupt attached but the pin permanently driven LOW, so the module's replies
     *        are never seen again (TX unaffected, which is why this bug was silent: bytes
     *        out looked correct, nothing ever came back).
     */
    virtual bool initPins() override {
        return true;
    }

    /**
     * @brief Assigns the configured audio-state table (copied in, clamped to MAX_STATES).
     *
     * @param states     Array of AudioState descriptors.
     * @param stateCount Number of valid entries in states.
     */
    void setStates(const AudioState *states, uint8_t stateCount)
    {
        _stateCount = min(stateCount, MAX_STATES);
        for (uint8_t i = 0; i < _stateCount; i++) _states[i] = states[i];
    }

    uint8_t getStateCount() const override { return _stateCount + 1; } // OFF + N states

    /**
     * @brief Read-only access to a configured AudioState by index, for JSON introspection
     *        (mirrors I2cPwmMotorDevice::getMotorState()) — used by DeviceApi's /api/devices
     *        to expose per-state labels so the cockpit can render one button per state
     *        instead of a single on/off toggle.
     *
     * @param i Index into the configured states (0-based, < getAudioStateCount()).
     */
    const AudioState &getAudioState(uint8_t i) const { return _states[i]; }

    /**
     * @brief Number of configured audio states (excludes the implicit OFF state 0),
     *        i.e. getStateCount() - 1. Use this to bound loops over getAudioState(i).
     */
    uint8_t getAudioStateCount() const { return _stateCount; }

    /**
     * @brief Coroutine loop: drives the volume fade/hold/auto-stop state machine.
     *
     * Mirrors I2cPwmMotorDevice::runCoroutine()'s ramp state machine (see that file's
     * documentation for the AceRoutine COROUTINE_DELAY/member-variable constraints, which
     * apply identically here): fade in to the state's target volume, hold for duration_ms
     * (0 = no auto-stop), fade out and stop playback, then auto-trigger OFF.
     */
    int runCoroutine() override;

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
    /**
     * @brief DCC accessory (on/off) dispatch — scenarios 1-2 from the design doc: play a
     * configured track or folder (with optional repeat/random/duration/fade) on "on",
     * stop on "off". Drives the same state machine as newState()/getTargetState() used by
     * runCoroutine(); State is mapped the same way I2cPwmMotorDevice does (0 = OFF, else
     * state 1).
     */
    virtual void setDccAccessoryState(uint8_t State) override
    {
        newState(State ? 1 : 0);
    }

    inline virtual void switchOn(bool skipDelay = false) override
    {
        if (_stateCount > 0) newState(1, skipDelay);
    }

    // Contrôle de lecture
    inline void playTrack(uint8_t trackNumber) { _lastTrack = trackNumber; bus->playTrack(trackNumber); }
    inline void nextTrack() { bus->nextTrack(); }
    inline void previousTrack() { bus->previousTrack(); }
    inline void pausePlayback() { bus->pausePlayback(); }
    inline void resumePlayback() { bus->resumePlayback(); }
    inline void stopPlayback() { bus->stopPlayback(); }

    // Volume
    inline void setVolume(uint8_t level)
    {
        volume = level;
        bus->setVolume(level);
    }
    inline void increaseVolume() { bus->increaseVolume(); }
    inline void decreaseVolume() { bus->decreaseVolume(); }

    // Modes de lecture
    inline void repeatPlayback(uint8_t trackNumber) { _lastTrack = trackNumber; bus->repeatPlayback(trackNumber); }
    inline void randomPlayback() { bus->randomPlayback(); }
    inline void continuousLoopPlayback(bool enable) { bus->continuousLoopPlayback(enable); }
    inline void setCurrentTrackLoop(bool enable) { bus->setCurrentTrackLoop(enable); }

    // Contrôle par dossier/fichier
    inline void playSpecificFolder(uint8_t folderNumber, uint8_t fileNumber)
    {
        bus->playSpecificFolder(folderNumber, fileNumber);
    }
    inline void compositePlayback(uint8_t fileSequence[], size_t length)
    {
        bus->compositePlayback(fileSequence, length);
    }

    // Gestion alimentation
    inline void resetModule() { bus->resetModule(); }
    inline void enterLowPowerMode() { bus->enterLowPowerMode(); }

    DfR1173Bus *bus = nullptr;         ///< Serial protocol layer for the DFR1173 module.
    DFAUDIO_VOLUME volume = 0;         ///< Current volume level (0–30).

private:
    uint8_t    _stateCount = 0;
    AudioState _states[MAX_STATES];
    uint8_t    _lastTrack = 1;  ///< Track number last started, reused by the F3 repeat-toggle mapping.

    // Members that survive across COROUTINE_DELAY suspension points — see
    // I2cPwmMotorDevice.h for why these cannot be runCoroutine() locals.
    uint8_t  _currentVolume  = 0;
    uint8_t  _targetVolume   = 0;
    uint8_t  _rampFromVolume = 0;
    uint32_t _rampStartMs    = 0;
    uint32_t _rampDurMs      = 0;
    uint32_t _runDurMs       = 0;
    uint32_t _runStartMs     = 0;
    uint32_t _stopFadeOutMs  = 0;
    uint32_t _ackWaitStartMs = 0;
    AudioOnEnd _onEnd        = AudioOnEnd::STOP;
    AudioStart _start        = AudioStart::FILE; ///< Snapshot of the active state's start mode (see runCoroutine()).
    uint8_t  _startNum       = 1; ///< Snapshot of the active state's start_num (see runCoroutine()).
    uint8_t  _folderFileNum  = 1; ///< Snapshot of the active state's folder_file_num (see runCoroutine()).
    bool     _manualAdvance  = false; ///< True while polling RX to chain to the next file (see runCoroutine()).
};

#endif // LFX_SERIAL_AUDIO_ENABLED
