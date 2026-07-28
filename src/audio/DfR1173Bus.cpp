/**
 * @file DfR1173Bus.cpp
 * @brief Implements the DfR1173Bus class (DFRobot DFR1173 serial protocol layer).
 *
 * Initializes the SoftwareSerial link to the module and implements the DFR1173 command
 * protocol (7-byte frames: 0x7E <cmd> <len_hi> <len_lo> <param_hi> <param_lo> 0xEF, no
 * checksum) for playback control, volume adjustment, playback modes, and power management.
 *
 * Reference: product page https://www.dfrobot.com/product-2862.html (DFR1173), wiki
 * https://wiki.dfrobot.com/dfr1173/docs/18471, official (header-only, no PlatformIO release)
 * driver https://github.com/DFRobot/DFRobot_SerialMP3, from which this file's frame layout
 * and command set are derived. Not the DFPlayer Mini/YX5200 protocol (10-byte frames, 16-bit
 * checksum) despite the similar module name.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-17
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#include "audio/DfR1173Bus.h"
#include "utils/utils.h"

#ifdef LFX_SERIAL_AUDIO_ENABLED

/**
 * @brief Constructs a DfR1173Bus instance and initializes the SoftwareSerial link.
 *
 * If no existing SoftwareSerial is provided (serial is nullptr), a new one is created on
 * rxPin/txPin and started at the given baud rate.
 *
 * @param serial  Pointer to an existing SoftwareSerial, or nullptr to allocate a new one.
 * @param rxPin   RX pin (connected to module TX).
 * @param txPin   TX pin (connected to module RX).
 * @param speed   Baud rate (default: 9600).
 */
DfR1173Bus::DfR1173Bus(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed)
{
    if (serial == nullptr)
    {
        serial = new SoftwareSerial(pinId(rxPin), pinId(txPin));
        serial->begin(speed);
        _ownsSerial = true;
    }
    this->serial = serial;
}

DfR1173Bus::~DfR1173Bus()
{
    if (_ownsSerial)
    {
        serial->end();
        delete serial;
    }
}

void DfR1173Bus::sendCommand(const uint8_t command[], size_t length)
{
    // Fire-and-forget: no blocking wait for a module ACK. A prior version
    // busy-waited up to 1s in waitForAck() after every setVolume() call —
    // stalling the whole firmware, including the AceRoutine scheduler, since
    // this is called synchronously from the coroutine path. Every other
    // command on this bus (playTrack, stopPlayback, ...) was already
    // fire-and-forget; volume just followed suit for consistency.
#ifdef DCC_AUDIT
    // Diagnostic only: drain and log whatever the module sent back since the
    // last command (ACK 7E 41 EF, error 7E 40 ..., etc. per the DFR1173 datasheet
    // section 3.2) before sending the next one, since nothing else on this bus
    // ever reads RX.
    if (serial->available()) {
        LOG_PRINT(F("[DfR1173] RX:"));
        while (serial->available()) {
            char buf[4];
            snprintf(buf, sizeof(buf), " %02X", serial->read());
            LOG_PRINT(buf);
        }
        LOG_PRINTLN(F(""));
    }
#endif
#ifdef DCC_AUDIT
    {
        LOG_PRINT(F("[DfR1173] TX:"));
        for (size_t i = 0; i < length; i++) {
            char buf[4];
            snprintf(buf, sizeof(buf), " %02X", command[i]);
            LOG_PRINT(buf);
        }
        LOG_PRINTLN(F(""));
    }
#endif
    for (size_t i = 0; i < length; i++)
    {
        serial->write(command[i]);
    }
}

bool DfR1173Bus::checkAck()
{
    bool ackSeen = false;
    while (serial->available()) {
        uint8_t b = serial->read();
        if (_ackState != AckState::WAIT_START && b == 0x7E) {
            _ackState = AckState::WAIT_CMD;
            continue;
        }
        switch (_ackState) {
            case AckState::WAIT_START:
                if (b == 0x7E) _ackState = AckState::WAIT_CMD;
                break;
            case AckState::WAIT_CMD:
                if (b == 0x41) ackSeen = true;
                _ackState = AckState::WAIT_START;
                break;
        }
    }
    return ackSeen;
}

void DfR1173Bus::debugDrainRx(uint32_t forMs)
{
#ifdef DCC_AUDIT
    uint32_t start = millis();
    bool any = false;
    while (millis() - start < forMs) {
        if (serial->available()) {
            if (!any) { LOG_PRINT(F("[DfR1173] DRAIN RX:")); any = true; }
            char buf[4];
            snprintf(buf, sizeof(buf), " %02X", serial->read());
            LOG_PRINT(buf);
        }
    }
    if (any) LOG_PRINTLN(F(""));
    else LOG_PRINTLN(F("[DfR1173] DRAIN RX: (nothing)"));
#else
    (void)forMs;
#endif
}

void DfR1173Bus::debugQueryStatus()
{
    uint8_t command[] = {0x7E, 0x42, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

bool DfR1173Bus::pollTrackFinished()
{
    static constexpr uint8_t CMD_PLAY_DONE_1 = 0x3D;
    static constexpr uint8_t CMD_PLAY_DONE_2 = 0x3E;
    // Standard DFR1173 frames are 0x7E <cmd> <len_hi> <len_lo> <data...> 0xEF (datasheet
    // §3/§4); the only exception is the ACK frame (§3.2.3), which is just 0x7E 0x41 0xEF
    // with no length/data field. Reading the length field per-frame (instead of assuming a
    // fixed 4-byte payload for everything) sizes the payload correctly for both shapes and
    // for any future command whose length isn't 0x00 0x02. No checksum exists, so re-sync
    // on any 0x7E seen out of place.

    bool finished = false;
    while (serial->available()) {
        uint8_t b = serial->read();
        if (_rxState != RxState::WAIT_START && b == 0x7E) {
            // Unexpected start byte mid-frame: re-sync rather than getting stuck.
            _rxState = RxState::WAIT_CMD;
            _rxPayloadLeft = 0;
            continue;
        }
        switch (_rxState) {
            case RxState::WAIT_START:
                if (b == 0x7E) _rxState = RxState::WAIT_CMD;
                break;
            case RxState::WAIT_CMD:
                _rxCmd = b;
                // ACK (0x41) has no length/data field at all — jump straight to the end byte.
                _rxState = (_rxCmd == 0x41) ? RxState::WAIT_END : RxState::WAIT_LEN_HI;
                break;
            case RxState::WAIT_LEN_HI:
                // Length is always 0x00 in every documented frame; ignored either way since
                // the low byte alone covers every real payload size (0-2 bytes).
                _rxState = RxState::WAIT_LEN_LO;
                break;
            case RxState::WAIT_LEN_LO:
                _rxPayloadLeft = b;
                _rxState = (_rxPayloadLeft == 0) ? RxState::WAIT_END : RxState::WAIT_PAYLOAD;
                break;
            case RxState::WAIT_PAYLOAD:
                if (--_rxPayloadLeft == 0) _rxState = RxState::WAIT_END;
                break;
            case RxState::WAIT_END:
                if (b == 0xEF && (_rxCmd == CMD_PLAY_DONE_1 || _rxCmd == CMD_PLAY_DONE_2)) {
                    finished = true;
                }
                _rxState = RxState::WAIT_START;
                break;
        }
    }
    return finished;
}

// Contrôle lecture
void DfR1173Bus::playTrack(uint8_t trackNumber)
{
    uint8_t command[] = {0x7E, 0x03, 0x00, 0x02, 0x00, trackNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::nextTrack()
{
    uint8_t command[] = {0x7E, 0x01, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::previousTrack()
{
    uint8_t command[] = {0x7E, 0x02, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::pausePlayback()
{
    uint8_t command[] = {0x7E, 0x0E, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::resumePlayback()
{
    uint8_t command[] = {0x7E, 0x0D, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::stopPlayback()
{
    uint8_t command[] = {0x7E, 0x16, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

// Volume
void DfR1173Bus::setVolume(uint8_t level)
{
    if (level > 30)
    {
        return;
    }
    uint8_t command[] = {0x7E, 0x06, 0x00, 0x02, 0x00, level, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::increaseVolume()
{
    uint8_t command[] = {0x7E, 0x04, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::decreaseVolume()
{
    uint8_t command[] = {0x7E, 0x05, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

// Modes de lecture
void DfR1173Bus::repeatPlayback(uint8_t trackNumber)
{
    uint8_t command[] = {0x7E, 0x08, 0x00, 0x02, 0x00, trackNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::randomPlayback()
{
    uint8_t command[] = {0x7E, 0x18, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::continuousLoopPlayback(bool enable)
{
    // 0x11 (datasheet §4.1.6): toggles whole-root-directory auto-advance-forever playback.
    // Not track-scoped — DL is just the on/off flag (0x01 start, 0x00 stop), unlike 0x08
    // (single-track loop, which takes a track number).
    uint8_t command[] = {0x7E, 0x11, 0x00, 0x02, 0x00, (uint8_t)(enable ? 0x01 : 0x00), 0xEF};
    sendCommand(command, sizeof(command));
}

// Folders & files
void DfR1173Bus::playSpecificFolder(uint8_t folderNumber, uint8_t fileNumber)
{
    uint8_t command[] = {0x7E, 0x0F, 0x00, 0x02, folderNumber, fileNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::compositePlayback(uint8_t fileSequence[], size_t length)
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
void DfR1173Bus::resetModule()
{
    uint8_t command[] = {0x7E, 0x0C, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

void DfR1173Bus::enterLowPowerMode()
{
    uint8_t command[] = {0x7E, 0x0A, 0x00, 0x02, 0x00, 0x01, 0xEF};
    sendCommand(command, sizeof(command));
}

#endif // LFX_SERIAL_AUDIO_ENABLED
