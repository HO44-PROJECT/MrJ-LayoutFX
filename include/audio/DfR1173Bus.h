/**
 * @file DfR1173Bus.h
 * @brief Defines the DfR1173Bus class implementing the DFRobot DFR1173 (MP3 Voice Prompter)
 *        serial protocol.
 *
 * Pure protocol layer: owns the SoftwareSerial link and implements the DFR1173's 7-byte
 * frame protocol (0x7E <cmd> <len_hi> <len_lo> <data...> 0xEF, no checksum) for playback
 * control, volume adjustment, playback modes, and power management. Holds no device/DCC
 * logic — see DfRobotSerialMP3 for the MultiplePinDevice wrapper that uses this bus.
 *
 * This is NOT the same protocol as the classic DFPlayer Mini/YX5200 module (10-byte frames
 * with a 16-bit checksum) — despite the similar name, the DFR1173 uses its own command set.
 * Reference: product page https://www.dfrobot.com/product-2862.html (DFR1173), wiki
 * https://wiki.dfrobot.com/dfr1173/docs/18471, official (header-only, no PlatformIO release)
 * driver https://github.com/DFRobot/DFRobot_SerialMP3, from which this class's frame layout
 * and command set are derived.
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

#include "devices/PinState.h"
#include <SoftwareSerial.h>

typedef uint8_t DFAUDIO_VOLUME;

/**
 * @class DfR1173Bus
 * @brief Implements the DFRobot DFR1173 serial command protocol over a SoftwareSerial link.
 *
 * One instance per physical module (each DFR1173 has its own serial link, unlike shared
 * buses such as Spi595Bus). Constructed with an existing SoftwareSerial, or allocates its
 * own on the given RX/TX pins.
 */
class DfR1173Bus
{
public:
    /**
     * @brief Constructs a DfR1173Bus and initializes the SoftwareSerial link.
     *
     * If no existing SoftwareSerial is provided (serial is nullptr), a new one is created on
     * rxPin/txPin and started at the given baud rate.
     *
     * @param serial  Pointer to an existing SoftwareSerial, or nullptr to allocate a new one.
     * @param rxPin   RX pin (connected to module TX).
     * @param txPin   TX pin (connected to module RX).
     * @param speed   Baud rate (default: 9600).
     */
    DfR1173Bus(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed = 9600);

    /**
     * @brief Detaches the SoftwareSerial's RX interrupt and frees it, but only if this
     *        instance allocated it itself (an externally-supplied serial is left alone —
     *        the caller still owns it).
     *
     * Without this, a config hot-reload (DeviceFactory::fullReset() -> reconstruct) leaks
     * the old SoftwareSerial with its GPIO interrupt still attached, while a new one is
     * immediately created on the same RX/TX pins — two live instances fighting over the
     * same pin's interrupt, silently corrupting TX/RX on the module's link.
     */
    ~DfR1173Bus();

    /**
     * @brief Envoie une commande brute au module.
     * @param command Tableau d'octets représentant la commande.
     * @param length Taille du tableau.
     */
    void sendCommand(const uint8_t command[], size_t length);

    /**
     * @brief Drains any bytes waiting on RX and reports whether a playback-complete
     *        frame (0x7E 0x3D/0x3E ... 0xEF, datasheet §3.2) was seen since the last call.
     *
     * Non-blocking: call repeatedly (e.g. once per coroutine tick) while a caller needs to
     * detect the end of the current file — used to drive AudioOnEnd::NEXT/RANDOM in
     * DfRobotSerialMP3, since nothing else on this bus reads RX in normal operation.
     *
     * @return true if a playback-complete frame was seen (and consumed) since the last call.
     */
    bool pollTrackFinished();

    /**
     * @brief Non-blocking check for the module's ACK frame (0x7E 0x41 0xEF, datasheet
     *        §3.2.3) sent after every command. Drains and discards any other bytes seen
     *        along the way (playback-complete frames are picked up separately by
     *        pollTrackFinished()).
     *
     * Call repeatedly from a COROUTINE_DELAY loop after sendCommand() — the module needs
     * real time to process a command before it can accept the next one; sending two
     * commands back-to-back with no gap made it silently drop one (no error, just never
     * acted on), which a fixed short delay works around but waiting for the actual ACK is
     * the correct fix per the protocol's own handshake design.
     *
     * @return true once the ACK has been seen (and consumed) since the last call.
     */
    bool checkAck();

#ifdef LFX_DCC_AUDIT_ENABLED
    /**
     * @brief Debug helper: blocking drain of RX for forMs, logging every byte seen.
     *        Only compiled under LFX_DCC_AUDIT_ENABLED (see sendCommand()'s TX/RX logging).
     */
    void debugDrainRx(uint32_t forMs);

    /**
     * @brief Debug helper: sends the 0x42 playback-status query for manual inspection.
     *        Only compiled under LFX_DCC_AUDIT_ENABLED.
     */
    void debugQueryStatus();

    /**
     * @brief Debug helper: sends the 0x4A "query total number of files in storage" command
     *        (datasheet §3.1/§4.2) for manual inspection — the total file count as the
     *        module's own physical index sees it, independent of what's visible in a file
     *        browser (see datasheet §5.3: playback order/indexing follows physical write
     *        time, not file name). Pair with debugDrainRx() to see the reply.
     */
    void debugQueryTotalFiles();

    /**
     * @brief Debug helper: sends the 0x4E "query current file index in storage" command
     *        (datasheet §3.1/§4.2) for manual inspection — reports which physical file
     *        index the module currently considers "current" (e.g. right after a
     *        playSpecificFolder() call), to check whether it landed where expected. Pair
     *        with debugDrainRx() to see the reply.
     */
    void debugQueryCurrentFileIndex();
#endif

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
    void continuousLoopPlayback(bool enable);

    /**
     * @brief Sends the 0x19 "set the currently playing track to loop playback" command
     *        (datasheet §4.1.8) — loops whatever is playing right now, no track number
     *        needed. Must be sent while the chip is actively playing (paused/stopped chip
     *        ignores it). Per the datasheet's own command table, the enable/disable data
     *        byte is inverted from what you'd expect: DL=0x00 enables the loop, DL=0x01
     *        disables it.
     *
     * @param enable true to enable single-track loop on the current track, false to disable.
     */
    void setCurrentTrackLoop(bool enable);

    // Contrôle par dossier/fichier
    void playSpecificFolder(uint8_t folderNumber, uint8_t fileNumber);
    void compositePlayback(uint8_t fileSequence[], size_t length);

    // Gestion alimentation
    void resetModule();
    void enterLowPowerMode();

    SoftwareSerial *serial = nullptr; ///< SoftwareSerial link to the DFR1173 module.

private:
    /// Byte-at-a-time parser state for pollTrackFinished(), persisted across calls since
    /// RX bytes can arrive split across multiple polls.
    ///
    /// The DFR1173 does not use a single fixed frame length: standard frames carry a 2-byte
    /// length field (datasheet §3/§4, always 0x00 0x02 in practice — one data word) making
    /// them 7 bytes total, but the ACK frame (§3.2.3, 0x7E 0x41 0xEF) has no length/data field
    /// at all and is only 3 bytes. WAIT_LEN reads that length field so the parser can size the
    /// payload per-frame instead of assuming every frame is the same shape, and treats the
    /// (undocumented) case of a length byte that would look like 0xEF as an ACK-shaped frame.
    enum class RxState : uint8_t { WAIT_START, WAIT_CMD, WAIT_LEN_HI, WAIT_LEN_LO, WAIT_PAYLOAD, WAIT_END };
    RxState _rxState       = RxState::WAIT_START;
    uint8_t _rxCmd         = 0;
    uint8_t _rxPayloadLeft = 0;
    bool    _ownsSerial    = false;

    /// Separate byte-at-a-time state for checkAck(), independent of _rxState/pollTrackFinished()
    /// since both scan the same RX stream for different frame types.
    enum class AckState : uint8_t { WAIT_START, WAIT_CMD };
    AckState _ackState = AckState::WAIT_START;
};

#endif // LFX_SERIAL_AUDIO_ENABLED
