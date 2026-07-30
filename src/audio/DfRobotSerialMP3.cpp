/**
 * @file DfRobotSerialMP3.cpp
 * @brief Implements the DfRobotSerialMP3 class, a MultiplePinDevice wrapper around a DFR1173
 *        audio module.
 *
 * Constructs the DfR1173Bus (serial protocol layer) and exposes it to the device/DCC
 * framework via MultiplePinDevice. See DfR1173Bus for the actual DFR1173 command protocol.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-17
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#include "audio/DfRobotSerialMP3.h"

#ifdef LFX_SERIAL_AUDIO_ENABLED

/**
 * @brief Constructs a DfRobotSerialMP3 instance and its underlying DfR1173Bus.
 *
 * @param serial  Pointer to an existing SoftwareSerial, or nullptr to allocate a new one.
 * @param rxPin   RX pin (connected to module TX).
 * @param txPin   TX pin (connected to module RX).
 * @param speed   Baud rate (default: 9600).
 */
DfRobotSerialMP3::DfRobotSerialMP3(SoftwareSerial *serial, PIN_ID rxPin, PIN_ID txPin, int speed)
{
    bus = new DfR1173Bus(serial, rxPin, txPin, speed);
}

/**
 * @brief Coroutine loop: processes state changes and drives the volume fade/hold sequence.
 *
 * State machine (mirrors I2cPwmMotorDevice::runCoroutine(), volume substituted for PWM µs):
 *   OFF_STATE : fade _currentVolume -> 0 using the last active state's fade_out_ms, then stop.
 *   1..N      : fade in to target volume (also starts playback), hold for duration_ms
 *               (0 = no auto-stop timer, holds until interrupted), fade out, stop playback,
 *               auto-trigger OFF.
 *
 * IMPORTANT: local variables declared inside this function do NOT survive COROUTINE_DELAY —
 * every value needed across a suspension point is a member variable (_xxx), not on the stack.
 * See I2cPwmMotorDevice.cpp for the detailed rationale (same AceRoutine constraint applies).
 *
 * @return 0 (AceRoutine convention).
 */
int DfRobotSerialMP3::runCoroutine() {
  COROUTINE_LOOP() {
    DEVICE_WAIT_STATE_CHANGE(getTargetState());
    DEVICE_APPLY_START_DELAY();

    if (getTargetState() == OFF_STATE) {
      // ── OFF: fade to silence, stop playback, then hold ──────────────────
      setState(OFF_STATE);

      if (_stopFadeOutMs > 0 && _currentVolume != 0) {
        _rampFromVolume = _currentVolume;
        _rampStartMs = millis();
        while (getTargetState() == OFF_STATE) {
          uint32_t elapsed = millis() - _rampStartMs;
          if (elapsed >= _stopFadeOutMs) { _currentVolume = 0; break; }
          _currentVolume = (uint8_t)((int32_t)_rampFromVolume -
            (int32_t)_rampFromVolume * (int32_t)elapsed / (int32_t)_stopFadeOutMs);
          setVolume(_currentVolume);
          // Pacing delay (~50Hz step rate for the volume ramp), unrelated to
          // WAIT_FOR_ACK()'s 5ms polling delay below — this one paces the fade itself,
          // not a wait for the module's response.
          COROUTINE_DELAY(20);
        }
      }
      _currentVolume = 0;
      setVolume(0);
      stopPlayback();
      _stopFadeOutMs = 0;

    } else {
      // ── Active state 1..N ─────────────────────────────────────────────
      uint8_t s;
      s = (uint8_t)getTargetState();
      LOG_PRINT(F("DBG DfRobotSerialMP3::runCoroutine target state="));
      LOG_PRINT(s);
      LOG_PRINT(F(" _stateCount="));
      LOG_PRINTLN(_stateCount);
      if (s < 1 || s > _stateCount) {
        setState(s); // out-of-range — commit without running
      } else {
        // Snapshot ALL state fields into member vars BEFORE any COROUTINE_DELAY — "as"
        // itself (a reference into _states[]) and the local "s" do not survive suspension
        // (COROUTINE_DELAY returns out of this function; on resume the stack is rebuilt
        // from scratch, so any local/reference taken before it is dangling — this crashed
        // with a LoadProhibited fault the first time "as" was read after WAIT_FOR_ACK()'s
        // COROUTINE_DELAY below). Only _xxx members may be read past this point.
        const AudioState &as = _states[s - 1];
        _stopFadeOutMs = as.fade_out_ms;
        _targetVolume  = as.volume;
        _rampDurMs     = as.fade_in_ms;
        _runDurMs      = as.duration_ms;
        _onEnd         = as.on_end;
        _start         = as.start;
        _startNum      = as.start_num;
        _folderFileNum = as.folder_file_num;
        // Manual "next" (one 0x01 per finished track, via RX polling) applies to FILE and
        // FOLDER starts: neither has a native "start here, then auto-advance" command — 0x01
        // (next track, §3.1) and 0x11 (continuous loop, §4.1.6) both advance by the device's
        // physical storage order with no notion of "starting point", so chaining has to be
        // driven by us, one track at a time, off the playback-complete frames. FIRST has no
        // single targeted file to chain from, so it keeps the native 0x11 continuous-loop
        // instead. "random" never needs RX polling either: the module's native
        // randomPlayback() (0x18) picks across its whole library on its own regardless of
        // start, so we never have to guess how many files exist.
        _manualAdvance = (_start == AudioStart::FILE || _start == AudioStart::FOLDER) && (_onEnd == AudioOnEnd::NEXT);

        setState(s);

        // The module needs to actually finish processing a command before the next one
        // arrives — sending two commands back-to-back with zero delay made it silently
        // drop one (ACK still came back, but nothing played). Rather than a fixed delay,
        // wait for the module's own ACK (0x7E 0x41 0xEF, datasheet §3.2.3) with a safety
        // timeout after EVERY command in this startup sequence, not just the last one.
        // A COROUTINE_DELAY inside must run directly in runCoroutine()'s own body (it's a
        // macro that suspends via labels in this function's switch, not a real function
        // call), so this has to stay a macro here rather than a member function.
        //
        // The COROUTINE_DELAY(5) below is a polling tick, not a fixed protocol wait: it just
        // yields to the scheduler between two checkAck() polls so this wait doesn't busy-loop
        // and starve every other coroutine (DCC decoding, other devices, etc.) for up to
        // 100ms. It is unrelated to — and not replaced by — the COROUTINE_DELAY(20) calls
        // used elsewhere in this function for fade/poll pacing (see their own comments).
#define WAIT_FOR_ACK() do { \
          _ackWaitStartMs = millis(); \
          while (!bus->checkAck() && millis() - _ackWaitStartMs < 100) { \
            COROUTINE_DELAY(5); \
          } \
        } while (0)

        // Clear any loop toggle left on from a previous state before starting this one —
        // 0x11 and 0x19 are standalone toggles, not implicitly reset by playTrack/
        // playSpecificFolder (datasheet §4.1.8 point 2 requires an explicit disable for
        // 0x19), so a prior folder/first + next/repeat state would otherwise keep
        // advancing/looping underneath this state's own playback.
        continuousLoopPlayback(false);
        WAIT_FOR_ACK();
        setCurrentTrackLoop(false);
        WAIT_FOR_ACK();

        // Set volume before playback so it's already correct when the file starts (no
        // audible pop/ramp for the no-fade case); the fade-in branch below re-sends it
        // progressively instead.
        if (_rampDurMs == 0) {
          _currentVolume = _targetVolume;
          setVolume(_currentVolume);
          WAIT_FOR_ACK();
        }

        // Start playback before/at the ramp so the fade-in is audible from track start.
        switch (_start) {
          case AudioStart::FOLDER:
            bus->playSpecificFolder(_startNum, _folderFileNum);
            break;
          case AudioStart::FIRST:
            playTrack(1);
            break;
          case AudioStart::FILE:
          default:
            playTrack(_startNum);
            break;
        }
        WAIT_FOR_ACK();

        if (_onEnd == AudioOnEnd::REPEAT) {
          // 0x08 (single-track loop, datasheet §4.1.3) takes a physical track number, which
          // is exactly what _startNum already is when _start == FILE. For FOLDER, the file
          // just started via playSpecificFolder() (0x0F) has no known physical track number
          // — but 0x19 (datasheet §4.1.8) loops whatever is playing right now with no track
          // number needed, so it repeats the exact file just started. FIRST has no single
          // targeted file (it's just "the module's first file"), so it keeps the old
          // whole-library continuous-loop behavior.
          if (_start == AudioStart::FILE) {
            repeatPlayback(_startNum);
          } else if (_start == AudioStart::FOLDER) {
            setCurrentTrackLoop(true);
          } else {
            continuousLoopPlayback(true);
          }
          WAIT_FOR_ACK();
        } else if (_onEnd == AudioOnEnd::NEXT && _start == AudioStart::FIRST) {
          continuousLoopPlayback(true);
          WAIT_FOR_ACK();
        } else if (_onEnd == AudioOnEnd::RANDOM) {
          randomPlayback();
          WAIT_FOR_ACK();
        }

#undef WAIT_FOR_ACK

        // ── Fade in ──────────────────────────────────────────────────────
        if (_rampDurMs > 0 && _currentVolume != _targetVolume) {
          _rampFromVolume = _currentVolume;
          _rampStartMs = millis();
          while (getState() > OFF_STATE) {
            uint32_t elapsed = millis() - _rampStartMs;
            if (elapsed >= _rampDurMs) { _currentVolume = _targetVolume; break; }
            _currentVolume = (uint8_t)((int32_t)_rampFromVolume +
              (int32_t)(_targetVolume - _rampFromVolume) * (int32_t)elapsed / (int32_t)_rampDurMs);
            setVolume(_currentVolume);
            // Pacing delay for the fade-in ramp (~50Hz step rate) — same role as the
            // OFF-state fade-out delay above, not a protocol wait.
            COROUTINE_DELAY(20);
          }
        } else {
          // No fade: volume was already set above, right before playTrack() — resending
          // the identical value here would be a redundant command with zero delay before
          // the next one, which the module can silently drop (see the ACK-wait above).
          _currentVolume = _targetVolume;
        }

        // ── Timed hold (skipped when interrupted; duration_ms == 0 means no
        //    auto-stop timer, so the effect holds until interrupted) — also runs
        //    whenever _manualAdvance is set, to poll RX for end-of-track and chain
        //    to the next file, even with no duration_ms cutoff. ──────────────────
        if (getState() > OFF_STATE && (_runDurMs > 0 || _manualAdvance)) {
          _runStartMs = millis();
          while (getState() > OFF_STATE) {
            if (_runDurMs > 0 && millis() - _runStartMs >= _runDurMs) break;
            if (_manualAdvance && bus->pollTrackFinished()) {
              // FILE: next physical track is _lastTrack + 1, kept in sync via playTrack()
              // so repeat (F3-style, 0x08) still has a valid track number to loop on. FOLDER:
              // no known physical track number to compute from (started via 0x0F, not a
              // physical index) — 0x01 (next track, §3.1) advances one file natively without
              // needing one.
              if (_start == AudioStart::FILE) {
                playTrack(_lastTrack + 1);
              } else {
                nextTrack();
              }
            }
            // Pacing delay for this loop's own polling rate (~50Hz): how often
            // pollTrackFinished() is checked and, when there's no _manualAdvance, simply how
            // often the duration_ms cutoff is re-evaluated. Not a protocol wait.
            COROUTINE_DELAY(20);
          }

          // ── Auto fade-out after timed hold (only if not interrupted) ────
          if (getState() > OFF_STATE) {
            if (_stopFadeOutMs > 0) {
              _rampFromVolume = _currentVolume;
              _rampStartMs = millis();
              while (getState() > OFF_STATE) {
                uint32_t elapsed = millis() - _rampStartMs;
                if (elapsed >= _stopFadeOutMs) { _currentVolume = 0; break; }
                _currentVolume = (uint8_t)((int32_t)_rampFromVolume -
                  (int32_t)_rampFromVolume * (int32_t)elapsed / (int32_t)_stopFadeOutMs);
                setVolume(_currentVolume);
                // Pacing delay for the auto fade-out ramp — same role as the other
                // fade loops' COROUTINE_DELAY(20) above, not a protocol wait.
                COROUTINE_DELAY(20);
              }
            } else {
              _currentVolume = 0;
            }
            // Auto-stop: trigger the OFF branch (only if the timed hold completed normally).
            if (getState() > OFF_STATE) {
              setVolume(0);
              stopPlayback();
              newState(OFF_STATE);
            }
          }
        }
        // No auto-stop timer (_runDurMs == 0): stays running until interrupted.
      }
    }
  }
  return 0;
}

#endif // LFX_SERIAL_AUDIO_ENABLED
