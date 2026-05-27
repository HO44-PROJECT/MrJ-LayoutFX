/**
 * @file I2cPwmMotorDevice.h
 * @brief Continuous-rotation motor device driven by a PCA9685 PWM controller via I²C.
 *
 * Maps to one PCA9685 channel.  A continuous-rotation servo expects:
 *   neutral_us → stop
 *   < neutral_us → one direction
 *   > neutral_us → opposite direction
 *
 * Speed [-100, +100] is linearly mapped:
 *   us = neutral_us - speed × 10
 * So speed=+50 → neutral−500 µs, speed=-50 → neutral+500 µs.
 *
 * State 0 = OFF — ramps down to neutral_us using the last active state's
 *                  ramp_down_ms, then stops.
 * States 1..N = MotorState[0..N-1] — runs through ramp-up, timed run, ramp-down.
 *   duration_ms = 0 → perpetual until newState(OFF) or next state.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_I2C_DEVICES_ENABLED

  #include <Adafruit_PWMServoDriver.h>
  #include "devices/Device.h"

class I2cPwmMotorDevice : public Device {
public:
  static const uint8_t MAX_STATES = 8;

  struct MotorState {
    int8_t   speed;        ///< Running speed [-100, +100]; sign = direction.
    uint32_t duration_ms;  ///< Run duration in ms (0 = perpetual until next newState).
    uint32_t ramp_up_ms;   ///< Ramp from current µs to target µs (0 = instant).
    uint32_t ramp_down_ms; ///< Ramp to neutral at end of run or on OFF (0 = instant).
    char     label[16];    ///< Display name (empty = "State N" fallback).
  };

  /**
   * @param pwm        Shared PCA9685 driver for this board.
   * @param channel    PCA9685 channel index (0–15).
   * @param states     Array of motor states (copied into the device).
   * @param stateCount Number of states (clamped to MAX_STATES).
   * @param neutralUs  PWM pulse width in µs for stop (default 1500). Calibrate
   *                   if the motor drifts when the stop command is sent.
   */
  I2cPwmMotorDevice(Adafruit_PWMServoDriver *pwm, uint8_t channel,
                    const MotorState *states, uint8_t stateCount,
                    uint16_t neutralUs = 1500);

  const __FlashStringHelper *getDeviceName() const override { return F("PCA9685Motor"); }
  uint8_t getStateCount() const override { return _stateCount + 1; } // OFF + N states

  int runCoroutine() override;

  size_t getPinCount() const override { return 1; }
  bool   setPin(size_t i, PIN_ID p) override;
  PIN_ID getPin(size_t i) const override;
  bool   initPins() override;

  void setDccAccessoryState(uint8_t state) override { newState(state ? 1 : 0); }
  void switchOn() override { if (_stateCount > 0) newState(1); }

  uint16_t          getNeutralUs() const               { return _neutralUs; }
  uint8_t           getMotorStateCount() const         { return _stateCount; }
  const MotorState &getMotorState(uint8_t idx) const   { return _states[idx]; }

private:
  Adafruit_PWMServoDriver *_pwm;
  uint8_t    _channel;
  uint8_t    _stateCount;
  MotorState _states[MAX_STATES];
  uint16_t   _neutralUs;

  // Members that survive across COROUTINE_DELAY suspension points.
  // IMPORTANT: local variables declared inside runCoroutine() are destroyed when the
  // coroutine suspends (COROUTINE_DELAY returns from the function). Any value used on
  // re-entry must be stored here, not on the stack.
  uint16_t _currentUs      = 0;  // current PWM µs (initialised to neutralUs in ctor)
  uint32_t _stopRampDownMs = 0;  // ramp_down_ms of the last active state (used for OFF ramp)
  uint16_t _targetUs       = 0;  // target PWM µs for current state's ramp-up
  uint16_t _rampFromUs     = 0;  // start PWM µs for current ramp phase
  uint32_t _rampStartMs    = 0;  // start time (ms) of current ramp phase
  uint32_t _rampDurMs      = 0;  // duration (ms) of current ramp-up
  uint32_t _runDurMs       = 0;  // duration_ms of current timed run (0 = perpetual)
  uint32_t _runStartMs     = 0;  // start time (ms) of current timed run

  uint16_t _speedToUs(int8_t speed) const {
    return (uint16_t)((int32_t)_neutralUs - (int32_t)speed * 10);
  }
};

#endif // MRJFX_I2C_DEVICES_ENABLED
