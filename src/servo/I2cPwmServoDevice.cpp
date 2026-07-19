/**
 * @file I2cPwmServoDevice.cpp
 * @brief Positional servo device driven by a PCA9685 PWM controller via I²C.
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#include "servo/I2cPwmServoDevice.h"

#ifdef LFX_I2C_DEVICES_ENABLED

I2cPwmServoDevice::I2cPwmServoDevice(Adafruit_PWMServoDriver *pwm, uint8_t channel,
                                     const Position *positions, uint8_t posCount,
                                     uint16_t pulseMinUs, uint16_t pulseMaxUs)
    : _pwm(pwm), _channel(channel), _posCount(min(posCount, MAX_POSITIONS)),
      _pulseMinUs(pulseMinUs), _pulseMaxUs(pulseMaxUs) {
  for (uint8_t i = 0; i < _posCount; i++)
    _positions[i] = positions[i];
  // _currentAngle stays at 0 (member default): the physical servo's actual angle
  // at boot is unknown, so the first slew always starts from 0°. After that,
  // _currentAngle is kept up to date by runCoroutine() and reflects the last
  // angle actually commanded, even across the auto-release to OFF_STATE.
}

bool I2cPwmServoDevice::initPins() {
  // Start de-energized: full-OFF on PCA9685, no PWM sent until a position is commanded.
  // Use INIT_STATE instead of OFF_STATE so applyDefaultStates() triggers the coroutine.
  if (_pwm) _pwm->setPWM(_channel, 0, 4096);
  setState(INIT_STATE);
  return true;
}

bool I2cPwmServoDevice::setPin(size_t i, PIN_ID p) {
  if (i != 0) return false;
  _channel = (uint8_t)pinId(p);
  return true;
}

PIN_ID I2cPwmServoDevice::getPin(size_t i) const {
  if (i != 0) return NO_PIN;
#ifdef LFX_SPI_CARDS_ENABLED
  return PIN_ID{_channel, 0};
#else
  return (PIN_ID)_channel;
#endif
}

int I2cPwmServoDevice::runCoroutine() {
  COROUTINE_LOOP() {
    // Wait until a state change is requested (state transitions to INIT_STATE).
    DEVICE_WAIT_STATE_CHANGE(getTargetState());
    DEVICE_APPLY_START_DELAY();

    if (getTargetState() == OFF_STATE) {
      // Stop: cut PWM so the servo de-energizes (goes limp).
      // 4096 sets the PCA9685 channel to full-OFF (permanently inactive).
      if (_pwm) _pwm->setPWM(_channel, 0, 4096);
      // Keep _currentAngle as-is: the servo was left at that angle (de-energized
      // but not moved), so the next position command slews from the true last
      // angle instead of always restarting from 0° (which caused a visible
      // 0°-then-target double move on every repeat command).
      setState(OFF_STATE);
    } else {
      uint8_t s = (uint8_t)getTargetState();
      if (s < 1 || s > _posCount) {
        setState(s); // out-of-range — commit without moving
      } else {
        // Start slewing to positions[s-1].
        _slewFrom       = _currentAngle;
        _slewTo         = _positions[s - 1].angle;
        _slewDurationMs = _positions[s - 1].duration_ms;
        _slewStartMs    = millis();
        _slewEaseOut    = _positions[s - 1].ease_out;
        setState(s);
#ifdef LFX_SERVO_SLEW_DEBUG
        Serial.printf("[servo CH%u] slew start: from=%d to=%d dur=%lums easeOut=%d now=%lu slewStartMs=%lu\n",
          _channel, (int)_slewFrom, (int)_slewTo, (unsigned long)_slewDurationMs, (int)_slewEaseOut,
          (unsigned long)millis(), (unsigned long)_slewStartMs);
#endif

        // Slew loop: runs every 20 ms until the target is reached or interrupted.
        // getState() drops to INIT_STATE (< 0) when a new newState() arrives,
        // which breaks the loop and allows the outer loop to process the new target.
        while (getState() > OFF_STATE && _currentAngle != _slewTo) {
          uint32_t nowMs = millis();
          uint32_t elapsed = nowMs - _slewStartMs;
          if (elapsed >= _slewDurationMs) {
            _currentAngle = _slewTo;
          } else if (_slewEaseOut) {
            // Quadratic ease-out: f(t) = 2t − t²  (fast start, decelerates at end).
            int32_t t        = (int32_t)elapsed * 1000 / (int32_t)_slewDurationMs;
            int32_t progress = t * (2000 - t) / 1000; // [0..1000] per-mille
            _currentAngle    = (int16_t)(_slewFrom + (int32_t)(_slewTo - _slewFrom) * progress / 1000);
          } else {
            _currentAngle = (int16_t)(_slewFrom +
              (int32_t)(_slewTo - _slewFrom) * elapsed / _slewDurationMs);
          }
          if (_pwm) _pwm->writeMicroseconds(_channel, _degreesToUs(_currentAngle));
#ifdef LFX_SERVO_SLEW_DEBUG
          Serial.printf("[servo CH%u] step: angle=%d us=%u now=%lu slewStartMs=%lu elapsed=%lu\n",
            _channel, (int)_currentAngle, _degreesToUs(_currentAngle),
            (unsigned long)nowMs, (unsigned long)_slewStartMs, (unsigned long)elapsed);
#endif
          COROUTINE_DELAY(20);
        }
        // Slew completed (non-interrupted): write exact final angle then auto-release.
        if (getState() > OFF_STATE) {
          _currentAngle = _slewTo;
          if (_pwm) _pwm->writeMicroseconds(_channel, _degreesToUs(_currentAngle));
#ifdef LFX_SERVO_SLEW_DEBUG
          Serial.printf("[servo CH%u] slew done: angle=%d us=%u -> auto-release\n",
            _channel, (int)_currentAngle, _degreesToUs(_currentAngle));
#endif
          newState(OFF_STATE); // triggers de-energize via the OFF_STATE branch above
        }
      }
    }
  }
  return 0;
}

#endif // LFX_I2C_DEVICES_ENABLED
