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

/**
 * @brief Detaches the SoftwareSerial's RX interrupt and frees it, but only if this instance
 *        allocated it itself (an externally-supplied serial is left alone — the caller still
 *        owns it). See the header's docblock for why this matters on a config hot-reload.
 */
DfR1173Bus::~DfR1173Bus()
{
    if (_ownsSerial)
    {
        serial->end();
        delete serial;
    }
}

/**
 * @brief Writes a raw command frame to the module, byte by byte.
 *
 * Fire-and-forget: no blocking wait for a module ACK. A prior version
 * busy-waited up to 1s in waitForAck() after every setVolume() call —
 * stalling the whole firmware, including the AceRoutine scheduler, since
 * this is called synchronously from the coroutine path. Every other
 * command on this bus (playTrack, stopPlayback, ...) was already
 * fire-and-forget; volume just followed suit for consistency. Use checkAck()
 * afterwards (from a coroutine poll loop) if the caller needs to know the
 * module actually processed the command.
 *
 * @param command Raw frame bytes to send (e.g. {0x7E, cmd, 0x00, 0x02, hi, lo, 0xEF}).
 * @param length  Number of bytes in command.
 */
void DfR1173Bus::sendCommand(const uint8_t command[], size_t length)
{
#ifdef LFX_DCC_AUDIT_ENABLED
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
#ifdef LFX_DCC_AUDIT_ENABLED
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

/**
 * @brief Non-blocking check for the module's ACK frame (0x7E 0x41 0xEF, datasheet §3.2.3)
 *        sent after every command. Drains and discards any other bytes seen along the way
 *        (playback-complete frames are picked up separately by pollTrackFinished()).
 *
 * Call repeatedly from a COROUTINE_DELAY loop after sendCommand() — the module needs real
 * time to process a command before it can accept the next one; sending two commands
 * back-to-back with no gap made it silently drop one (no error, just never acted on), which
 * a fixed short delay works around but waiting for the actual ACK is the correct fix per the
 * protocol's own handshake design.
 *
 * @return true once the ACK has been seen (and consumed) since the last call.
 */
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

#ifdef LFX_DCC_AUDIT_ENABLED
/**
 * @brief Debug helper: blocking drain of RX for forMs, logging every byte seen. Only compiled
 *        under LFX_DCC_AUDIT_ENABLED — unlike every other method in this class, this one
 *        busy-waits on purpose (it's a manual diagnostic tool, not part of the coroutine path).
 *
 * @param forMs How long to keep draining/logging, in milliseconds.
 */
void DfR1173Bus::debugDrainRx(uint32_t forMs)
{
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
}

/**
 * @brief Debug helper: sends the 0x42 "query playback status" command (datasheet §3.1/§4.2)
 *        for manual inspection. Pair with debugDrainRx() to see the reply.
 */
void DfR1173Bus::debugQueryStatus()
{
    uint8_t command[] = {0x7E, 0x42, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Debug helper: sends the 0x4A "query total number of files in storage" command
 *        (datasheet §3.1/§4.2) for manual inspection — the total file count as the module's
 *        own physical index sees it, independent of what's visible in a file browser (see
 *        datasheet §5.3: playback order/indexing follows physical write time, not file name).
 *        Pair with debugDrainRx() to see the reply.
 */
void DfR1173Bus::debugQueryTotalFiles()
{
    uint8_t command[] = {0x7E, 0x4A, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Debug helper: sends the 0x4E "query current file index in storage" command
 *        (datasheet §3.1/§4.2) for manual inspection — reports which physical file index the
 *        module currently considers "current" (e.g. right after a playSpecificFolder() call),
 *        to check whether it landed where expected. Pair with debugDrainRx() to see the reply.
 */
void DfR1173Bus::debugQueryCurrentFileIndex()
{
    uint8_t command[] = {0x7E, 0x4E, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}
#endif

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
/**
 * @brief Sends the 0x03 "play a specific track by physical index" command (datasheet §4.1.1).
 *
 * The track number is the module's own physical storage index (write order on the SD/flash),
 * not a filename — see the class-level note on datasheet §5.3 indexing. This is the command
 * used for AudioStart::FILE and AudioStart::FIRST (with trackNumber == 1).
 *
 * @param trackNumber Physical track index to play (1-based).
 */
void DfR1173Bus::playTrack(uint8_t trackNumber)
{
    uint8_t command[] = {0x7E, 0x03, 0x00, 0x02, 0x00, trackNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x01 "advance to next track" command (datasheet §3.1) — steps one file
 *        forward in the module's physical storage order, wrapping around the whole library.
 *        No target track number: whatever is "current" on the module advances by one.
 */
void DfR1173Bus::nextTrack()
{
    uint8_t command[] = {0x7E, 0x01, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x02 "step back to previous track" command (datasheet §3.1) — the mirror
 *        of nextTrack(), one file backward in physical storage order.
 */
void DfR1173Bus::previousTrack()
{
    uint8_t command[] = {0x7E, 0x02, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x0E "pause playback" command (datasheet §4.1.5) — freezes the current
 *        file mid-playback; resumePlayback() continues from the same position.
 */
void DfR1173Bus::pausePlayback()
{
    uint8_t command[] = {0x7E, 0x0E, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x0D "resume playback" command (datasheet §4.1.4) — continues a file
 *        previously paused with pausePlayback().
 */
void DfR1173Bus::resumePlayback()
{
    uint8_t command[] = {0x7E, 0x0D, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x16 "stop playback" command (datasheet §4.1.7) — stops outright (unlike
 *        pausePlayback(), there is no resume-from-here after this; the next playTrack()/
 *        playSpecificFolder() starts fresh).
 */
void DfR1173Bus::stopPlayback()
{
    uint8_t command[] = {0x7E, 0x16, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

// Volume
/**
 * @brief Sends the 0x06 "set volume" command (datasheet §4.1.2). Silently ignored (no
 *        command sent) if level is out of the module's supported 0-30 range, rather than
 *        clamping — callers (DfRobotSerialMP3) are expected to clamp/validate beforehand.
 *
 * @param level Target volume, 0 (silent) to 30 (max). Values above 30 are dropped.
 */
void DfR1173Bus::setVolume(uint8_t level)
{
    if (level > 30)
    {
        return;
    }
    uint8_t command[] = {0x7E, 0x06, 0x00, 0x02, 0x00, level, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x04 "step volume up one increment" command (datasheet §4.1.2 variant) —
 *        relative adjustment, no explicit level; the module clamps internally at its max (30).
 */
void DfR1173Bus::increaseVolume()
{
    uint8_t command[] = {0x7E, 0x04, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x05 "step volume down one increment" command (datasheet §4.1.2 variant) —
 *        relative adjustment, mirror of increaseVolume().
 */
void DfR1173Bus::decreaseVolume()
{
    uint8_t command[] = {0x7E, 0x05, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

// Modes de lecture
/**
 * @brief Sends the 0x08 "loop a specific track by physical index" command (datasheet §4.1.3)
 *        — unlike setCurrentTrackLoop(), this targets an explicit track number rather than
 *        "whatever is currently playing", so it can be sent standalone without first starting
 *        playback via playTrack().
 *
 * @param trackNumber Physical track index to loop (1-based).
 */
void DfR1173Bus::repeatPlayback(uint8_t trackNumber)
{
    uint8_t command[] = {0x7E, 0x08, 0x00, 0x02, 0x00, trackNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x18 "random playback" command (datasheet §4.1.9) — the module picks
 *        across its whole library on its own; no track count or range needs to be known
 *        by the caller.
 */
void DfR1173Bus::randomPlayback()
{
    uint8_t command[] = {0x7E, 0x18, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x11 "whole-root-directory continuous loop" command (datasheet §4.1.6) —
 *        toggles auto-advance-forever playback across the module's whole library. Not
 *        track-scoped — DL is just the on/off flag (0x01 start, 0x00 stop), unlike
 *        repeatPlayback() (single-track loop, which takes a track number).
 *
 * @param enable true to start continuous auto-advance, false to stop it.
 */
void DfR1173Bus::continuousLoopPlayback(bool enable)
{
    uint8_t command[] = {0x7E, 0x11, 0x00, 0x02, 0x00, (uint8_t)(enable ? 0x01 : 0x00), 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x19 "loop the currently playing track" command (datasheet §4.1.8) — loops
 *        whatever is playing right now, no track number needed, unlike repeatPlayback()
 *        (single-track loop by physical track number) or continuousLoopPlayback() (whole root
 *        directory). Per the datasheet's own command table, the enable/disable data byte is
 *        inverted from what you'd expect: DL=0x00 enables the loop, DL=0x01 disables it.
 *
 * @param enable true to enable single-track loop on the current track, false to disable.
 */
void DfR1173Bus::setCurrentTrackLoop(bool enable)
{
    uint8_t command[] = {0x7E, 0x19, 0x00, 0x02, 0x00, (uint8_t)(enable ? 0x00 : 0x01), 0xEF};
    sendCommand(command, sizeof(command));
}

// Folders & files
/**
 * @brief Sends the 0x0F "play a specific file within a specific folder" command (datasheet
 *        §4.1.10) — unlike playTrack()'s flat physical index, this addresses a file by its
 *        folder/file pair (the module's folder-relative numbering), used for
 *        AudioStart::FOLDER.
 *
 * @param folderNumber Folder number (1-255).
 * @param fileNumber   File number within that folder (1-255).
 */
void DfR1173Bus::playSpecificFolder(uint8_t folderNumber, uint8_t fileNumber)
{
    uint8_t command[] = {0x7E, 0x0F, 0x00, 0x02, folderNumber, fileNumber, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x21 "composite/sequence playback" command (datasheet §4.1.11) — plays a
 *        caller-supplied list of physical track numbers back-to-back in the given order, as
 *        a single command (unlike chaining playTrack() calls one at a time). Not currently
 *        driven by DfRobotSerialMP3's state machine (no AudioState field maps to an arbitrary
 *        sequence) — exposed for future use or direct/manual invocation.
 *
 * @param fileSequence Array of physical track numbers to play in order.
 * @param length       Number of entries in fileSequence.
 */
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
/**
 * @brief Sends the 0x0C "reset module" command (datasheet §4.1.4 variant) — reinitializes
 *        the DFR1173 as if power-cycled. Not used by DfRobotSerialMP3's normal operation;
 *        exposed for manual recovery/diagnostics.
 */
void DfR1173Bus::resetModule()
{
    uint8_t command[] = {0x7E, 0x0C, 0x00, 0x02, 0x00, 0x00, 0xEF};
    sendCommand(command, sizeof(command));
}

/**
 * @brief Sends the 0x0A "enter low-power mode" command (datasheet §4.1.4 variant, DL=0x01).
 *        Not used by DfRobotSerialMP3's normal operation; exposed for future power-management
 *        use (e.g. shutting the module down when idle for long periods).
 */
void DfR1173Bus::enterLowPowerMode()
{
    uint8_t command[] = {0x7E, 0x0A, 0x00, 0x02, 0x00, 0x01, 0xEF};
    sendCommand(command, sizeof(command));
}

#endif // LFX_SERIAL_AUDIO_ENABLED
