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
      if (s < 1 || s > _stateCount) {
        setState(s); // out-of-range — commit without running
      } else {
        // Snapshot all state fields into member vars BEFORE any COROUTINE_DELAY.
        const AudioState &as = _states[s - 1];
        _stopFadeOutMs = as.fade_out_ms;
        _targetVolume  = as.volume;
        _rampDurMs     = as.fade_in_ms;
        _runDurMs      = as.duration_ms;
        _onEnd         = as.on_end;
        // Manual "next" (file-by-file via RX polling) only applies when starting from a
        // single, specific file: the module has no "start at file N, then auto-advance"
        // command (0x11 continuous-loop is root-directory-wide with no start-track param —
        // datasheet §4.1.6), so chaining from a specific file has to be done by us, one
        // track at a time, off the playback-complete frames. When start is folder/first,
        // 0x11 already does exactly what NEXT means (advance through everything, looping
        // forever) so no RX polling is needed. "random" never needs RX polling either: the
        // module's native randomPlayback() (0x18) picks across its whole library on its own
        // regardless of start, so we never have to guess how many files exist.
        _manualAdvance = (as.start == AudioStart::FILE) && (as.on_end == AudioOnEnd::NEXT);

        setState(s);

        // The module needs to actually finish processing a command before the next one
        // arrives — sending two commands back-to-back with zero delay made it silently
        // drop one (ACK still came back, but nothing played). Rather than a fixed delay,
        // wait for the module's own ACK (0x7E 0x41 0xEF, datasheet §3.2.3) with a safety
        // timeout after EVERY command in this startup sequence, not just the last one.
        // A COROUTINE_DELAY inside must run directly in runCoroutine()'s own body (it's a
        // macro that suspends via labels in this function's switch, not a real function
        // call), so this has to stay a macro here rather than a member function.
#define WAIT_FOR_ACK() do { \
          _ackWaitStartMs = millis(); \
          while (!bus->checkAck() && millis() - _ackWaitStartMs < 100) { \
            COROUTINE_DELAY(5); \
          } \
        } while (0)

        // Clear any continuous-loop toggle left on from a previous state before starting
        // this one — 0x11 is a standalone toggle (not implicitly reset by playTrack/
        // playSpecificFolder), so a prior folder/first + next/repeat state would otherwise
        // keep advancing/looping underneath this state's own playback.
        continuousLoopPlayback(false);
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
        switch (as.start) {
          case AudioStart::FOLDER:
            bus->playSpecificFolder(as.start_num, 1);
            break;
          case AudioStart::FIRST:
            playTrack(1);
            break;
          case AudioStart::FILE:
          default:
            playTrack(as.start_num);
            break;
        }
        WAIT_FOR_ACK();

        if (as.on_end == AudioOnEnd::REPEAT) {
          // 0x08 (single-track loop) only makes sense for a specific file; for folder/first
          // "repeat" reads as "keep looping through everything" since there's no single
          // starting track to loop on, so it maps to the same native continuous-loop as NEXT.
          if (as.start == AudioStart::FILE) {
            repeatPlayback(as.start_num);
          } else {
            continuousLoopPlayback(true);
          }
          WAIT_FOR_ACK();
        } else if (as.on_end == AudioOnEnd::NEXT && as.start != AudioStart::FILE) {
          continuousLoopPlayback(true);
          WAIT_FOR_ACK();
        } else if (as.on_end == AudioOnEnd::RANDOM) {
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
              playTrack(_lastTrack + 1);
            }
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
