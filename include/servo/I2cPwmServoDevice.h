/**
 * @file I2cPwmServoDevice.h
 * @brief Positional servo device driven by a PCA9685 PWM controller via I²C.
 *
 * Supports up to MAX_POSITIONS configurable positions, each with a signed angle
 * (−90°..+90°, 0° = neutral/centre) and a transition duration.  The firmware slews
 * the servo smoothly over the specified duration using the AceRoutine coroutine scheduler.
 *
 * State 0 = OFF / emergency stop — freezes the servo at its current angle.
 * States 1..N = positions[0..N-1] — servo slews to the target angle.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_I2C_DEVICES_ENABLED

  #include <Adafruit_PWMServoDriver.h>
  #include "devices/Device.h"

class I2cPwmServoDevice : public Device {
public:
  static const uint8_t MAX_POSITIONS = 8;

  struct Position {
    int16_t  angle;        ///< Target angle in degrees (−90..+90, 0 = neutral/centre).
    uint32_t duration_ms;  ///< Transition duration in milliseconds.
    char     label[16];    ///< Display name shown on cockpit button (empty = "Pos N" fallback).
    bool     ease_out;     ///< Quadratic ease-out: fast start, decelerates at end (e.g. door/gate).
  };

  /**
   * @param pwm         Shared PCA9685 driver for this board.
   * @param channel     PCA9685 channel index (0–15).
   * @param positions   Array of positions (copied into the device).
   * @param posCount    Number of positions (clamped to MAX_POSITIONS).
   * @param pulseMinUs  PWM pulse width for −90° (µs). Default 1000 µs.
   * @param pulseMaxUs  PWM pulse width for +90° (µs). Default 2000 µs.
   *
   * Common values:
   *   SG90 / MG90S (standard hobby): 1000–2000 µs (default)
   *   Extended-range servos:          500–2500 µs
   */
  I2cPwmServoDevice(Adafruit_PWMServoDriver *pwm, uint8_t channel,
                    const Position *positions, uint8_t posCount,
                    uint16_t pulseMinUs = 1000, uint16_t pulseMaxUs = 2000);

  const __FlashStringHelper *getDeviceName() const override { return F("PCA9685Servo"); }
  uint8_t getStateCount() const override { return _posCount + 1; } // OFF + N positions

  int runCoroutine() override;

  size_t getPinCount() const override { return 1; }
  bool   setPin(size_t i, PIN_ID p) override;
  PIN_ID getPin(size_t i) const override;
  bool   initPins() override;

  void setDccAccessoryState(uint8_t state) override { newState((STATE_TYPE)state); }
  void switchOn() override { if (_posCount > 0) newState(1); }

  uint8_t         getPosCount() const              { return _posCount; }
  const Position &getPosition(uint8_t idx) const   { return _positions[idx]; }
  int16_t         getCurrentAngle() const          { return _currentAngle; }
  uint16_t        getPulseMinUs() const            { return _pulseMinUs; }
  uint16_t        getPulseMaxUs() const            { return _pulseMaxUs; }

private:
  Adafruit_PWMServoDriver *_pwm;
  uint8_t  _channel;
  uint8_t  _posCount;
  Position _positions[MAX_POSITIONS];
  uint16_t _pulseMinUs; ///< PWM µs for −90° (default 1000).
  uint16_t _pulseMaxUs; ///< PWM µs for +90° (default 2000).

  // Members that survive across COROUTINE_DELAY suspension points.
  int16_t  _currentAngle   = 0;
  int16_t  _slewFrom       = 0;
  int16_t  _slewTo         = 0;
  uint32_t _slewDurationMs = 0;
  uint32_t _slewStartMs    = 0;
  bool     _slewEaseOut    = false;

  uint16_t _degreesToUs(int16_t degrees) const {
    // Linear mapping: −90° → _pulseMinUs, 0° → mid, +90° → _pulseMaxUs.
    int32_t mid = ((int32_t)_pulseMinUs + _pulseMaxUs) / 2;
    int32_t half = ((int32_t)_pulseMaxUs - _pulseMinUs) / 2;
    int32_t us = mid + (int32_t)degrees * half / 90;
    if (us < _pulseMinUs) us = _pulseMinUs;
    if (us > _pulseMaxUs) us = _pulseMaxUs;
    return (uint16_t)us;
  }
};

#endif // MRJFX_I2C_DEVICES_ENABLED
