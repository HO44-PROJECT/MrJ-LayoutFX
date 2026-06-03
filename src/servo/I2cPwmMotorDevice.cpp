/**
 * @file I2cPwmMotorDevice.cpp
 * @brief Continuous-rotation motor device driven by a PCA9685 PWM controller via I²C.
 *
 * Implements the coroutine state machine for continuous-rotation servos:
 *   OFF   → ramp down to neutral_us, then hold (motor stops).
 *   1..N  → ramp up, timed or perpetual run, optional ramp down, then auto-OFF.
 *
 * Unlike I2cPwmServoDevice, the motor is never de-energized: it always receives
 * a PWM pulse (neutral_us at rest).  The "stop" command ramps to neutral rather
 * than cutting PWM.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "servo/I2cPwmMotorDevice.h"

#ifdef MRJFX_I2C_DEVICES_ENABLED

/**
 * @brief Constructs an I2cPwmMotorDevice for one channel of a PCA9685 board.
 *
 * Copies the state array (clamped to MAX_STATES) and stores the neutral pulse width.
 * The motor is not powered here; call initPins() after the PCA9685 driver is ready.
 *
 * @param pwm        Shared PCA9685 driver (must outlive this object).
 * @param channel    PCA9685 channel index (0–15).
 * @param states     Array of MotorState descriptors (copied into the device).
 * @param stateCount Number of states in the array (clamped to MAX_STATES).
 * @param neutralUs  PWM pulse width in µs that stops the motor (default 1500).
 */
I2cPwmMotorDevice::I2cPwmMotorDevice(Adafruit_PWMServoDriver *pwm, uint8_t channel,
                                     const MotorState *states, uint8_t stateCount,
                                     uint16_t neutralUs)
    : _pwm(pwm), _channel(channel),
      _stateCount(min(stateCount, MAX_STATES)),
      _neutralUs(neutralUs),
      _currentUs(neutralUs),
      _stopRampDownMs(0) {
  for (uint8_t i = 0; i < _stateCount; i++)
    _states[i] = states[i];
}

/**
 * @brief Initialises the PCA9685 channel and sets the device to INIT_STATE.
 *
 * Sends full-OFF (4096) to de-energize the motor completely at boot.
 * Does NOT call setState(OFF_STATE) — leaves the device in INIT_STATE so that
 * applyDefaultStates() can trigger the coroutine transition to the actual default.
 * This ensures the neutral pulse is sent even when default_state is "off".
 *
 * @return true (always succeeds if the PCA9685 driver was initialised beforehand).
 */
bool I2cPwmMotorDevice::initPins() {
  if (_pwm) _pwm->setPWM(_channel, 0, 4096); // Full-OFF (de-energize)
  setState(INIT_STATE); // Transitional state — will transition to defaultState
  return true;
}

/**
 * @brief Assigns a PCA9685 channel to this device.
 *
 * Only index 0 is valid; this is a single-channel device.
 *
 * @param i Pin index (must be 0).
 * @param p PIN_ID carrying the channel number.
 * @return true if i == 0, false otherwise.
 */
bool I2cPwmMotorDevice::setPin(size_t i, PIN_ID p) {
  if (i != 0) return false;
  _channel = (uint8_t)pinId(p);
  return true;
}

/**
 * @brief Returns the PIN_ID for the assigned PCA9685 channel.
 *
 * @param i Pin index (must be 0).
 * @return PIN_ID for the channel, or NO_PIN if i != 0.
 */
PIN_ID I2cPwmMotorDevice::getPin(size_t i) const {
  if (i != 0) return NO_PIN;
#ifdef MRJFX_SPI_CARDS_ENABLED
  return PIN_ID{_channel, 0};
#else
  return (PIN_ID)_channel;
#endif
}

/**
 * @brief Coroutine loop: processes state changes and drives ramp/run sequences.
 *
 * State machine:
 *   - OFF_STATE : ramp _currentUs → _neutralUs over _stopRampDownMs, then hold.
 *   - 1..N      : ramp up to target speed, run for duration_ms (0 = perpetual),
 *                 then ramp down and auto-trigger OFF_STATE.
 *
 * IMPORTANT: local variables declared inside this function do NOT survive
 * COROUTINE_DELAY — the function returns and is re-entered at the resume label,
 * skipping all declarations.  Every value needed across a suspension point is
 * stored in a member variable (_xxx), not on the stack.
 *
 * @return 0 (AceRoutine convention).
 */
int I2cPwmMotorDevice::runCoroutine() {
  COROUTINE_LOOP() {
    // Wait until a state change is requested (target transitions out of current state).
    DEVICE_WAIT_STATE_CHANGE(getTargetState());

    if (getTargetState() == OFF_STATE) {
      // ── OFF: ramp to neutral then hold ────────────────────────────────────
      // Release busy flag first so new commands can queue during the ramp-down.
      setState(OFF_STATE);

      if (_stopRampDownMs > 0 && _currentUs != _neutralUs) {
        _rampFromUs  = _currentUs;
        _rampStartMs = millis();
        while (getTargetState() == OFF_STATE) {
          uint32_t elapsed = millis() - _rampStartMs;
          if (elapsed >= _stopRampDownMs) { _currentUs = _neutralUs; break; }
          _currentUs = (uint16_t)((int32_t)_rampFromUs +
            (int32_t)(_neutralUs - _rampFromUs) * (int32_t)elapsed / (int32_t)_stopRampDownMs);
          if (_pwm) _pwm->writeMicroseconds(_channel, _currentUs);
          COROUTINE_DELAY(20);
        }
      }
      // Hold neutral (motor stopped).
      _currentUs = _neutralUs;
      if (_pwm) _pwm->writeMicroseconds(_channel, _neutralUs);
      _stopRampDownMs = 0;

    } else {
      // ── Active state 1..N ─────────────────────────────────────────────────
      uint8_t s = (uint8_t)getTargetState();
      if (s < 1 || s > _stateCount) {
        setState(s); // out-of-range — commit without running
      } else {
        // Snapshot all state fields into member vars BEFORE any COROUTINE_DELAY.
        _stopRampDownMs = _states[s - 1].ramp_down_ms;
        _targetUs       = _speedToUs(_states[s - 1].speed);
        _rampDurMs      = _states[s - 1].ramp_up_ms;
        _runDurMs       = _states[s - 1].duration_ms;

        // Mark running — allows newState() to interrupt during ramps and run.
        setState(s);

        // ── Ramp up ──────────────────────────────────────────────────────────
        if (_rampDurMs > 0 && _currentUs != _targetUs) {
          _rampFromUs  = _currentUs;
          _rampStartMs = millis();
          while (getState() > OFF_STATE) {
            uint32_t elapsed = millis() - _rampStartMs;
            if (elapsed >= _rampDurMs) { _currentUs = _targetUs; break; }
            _currentUs = (uint16_t)((int32_t)_rampFromUs +
              (int32_t)(_targetUs - _rampFromUs) * (int32_t)elapsed / (int32_t)_rampDurMs);
            if (_pwm) _pwm->writeMicroseconds(_channel, _currentUs);
            COROUTINE_DELAY(20);
          }
        } else {
          _currentUs = _targetUs;
        }
        // Write final value (mirrors ramp-down pattern; covers both instant and
        // ramp paths; ensures the loop's break-on-elapsed writes _targetUs to PWM).
        if (_pwm) _pwm->writeMicroseconds(_channel, _currentUs);

        // ── Timed run (skipped when interrupted or perpetual) ─────────────────
        if (getState() > OFF_STATE && _runDurMs > 0) {
          _runStartMs = millis();
          while (getState() > OFF_STATE) {
            if (millis() - _runStartMs >= _runDurMs) break;
            COROUTINE_DELAY(20);
          }

          // ── Auto ramp-down after timed run (only if not interrupted) ─────────
          if (getState() > OFF_STATE) {
            if (_stopRampDownMs > 0) {
              _rampFromUs  = _currentUs;
              _rampStartMs = millis();
              while (getState() > OFF_STATE) {
                uint32_t elapsed = millis() - _rampStartMs;
                if (elapsed >= _stopRampDownMs) { _currentUs = _neutralUs; break; }
                _currentUs = (uint16_t)((int32_t)_rampFromUs +
                  (int32_t)(_neutralUs - _rampFromUs) * (int32_t)elapsed / (int32_t)_stopRampDownMs);
                if (_pwm) _pwm->writeMicroseconds(_channel, _currentUs);
                COROUTINE_DELAY(20);
              }
            } else {
              _currentUs = _neutralUs;
            }
            // Auto-stop: trigger the OFF branch (only if timed run completed normally).
            if (getState() > OFF_STATE) {
              if (_pwm) _pwm->writeMicroseconds(_channel, _neutralUs);
              newState(OFF_STATE);
            }
          }
        }
        // Perpetual (_runDurMs == 0): stays running; outer loop waits for next command.
      }
    }
  }
  return 0;
}

#endif // MRJFX_I2C_DEVICES_ENABLED
