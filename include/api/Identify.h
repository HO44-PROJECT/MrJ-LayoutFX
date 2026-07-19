/**
 * @file Identify.h
 * @brief Non-blocking LED "identify" blinker — physically locate a wired output.
 *
 * One target at a time (a GPIO pin OR an SPI 74HC595 channel). Blinks a
 * distinctive pattern (three short flashes + a pause) so the user can spot the
 * connected LED on a complex layout. Driven from LayoutFX::loop() on Core 1, so it
 * stays coherent with the coroutine scheduler and the SPI flush.
 *
 * Started/stopped by the WebUI "Identify" button via POST /api/test/identify.
 * Runs until explicitly stopped (no timeout).
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */
#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_API_SERVER_ENABLED

  #include <Arduino.h>

class Identify {
public:
  /** @brief Start blinking a raw MCU GPIO pin (replaces any current target). */
  static void startGpio(uint8_t pin);

  /**
   * @brief Charlieplex wiring test: blink @p testPin HIGH while holding the
   *        other candidate pins LOW, so a single LED of a charlieplexed signal
   *        lights predictably even before the device is configured. Used by the
   *        DB-signal wiring assistant in the device editor.
   * @param testPin   GPIO driven with the blink pattern.
   * @param lowPins   Other candidate GPIOs to hold LOW (the signal's other wires).
   * @param lowCount  Number of entries in @p lowPins (clamped to kMaxLow).
   */
  static void startCharlieplex(uint8_t testPin, const uint8_t *lowPins, uint8_t lowCount);

  #ifdef LFX_SPI_CARDS_ENABLED
  /** @brief Start blinking a 74HC595 channel (card is 1-based). */
  static void startSpi(uint8_t card, uint8_t channel);
  #endif

  /** @brief Stop blinking and drive the current target inactive. */
  static void stop();

  /** @brief Advance the blink pattern — call every LayoutFX::loop() iteration. */
  static void loop();

  /** @brief True while a target is being identified. */
  static bool active() { return _mode != NONE; }

private:
  enum Mode : uint8_t { NONE, GPIO_MODE, SPI_MODE, CHARLIE_MODE };
  static constexpr uint8_t kMaxLow = 7; ///< Max "other" pins held LOW in charlieplex mode.
  static Mode     _mode;
  static uint8_t  _pin;        ///< GPIO pin number, or SPI channel.
  static uint8_t  _card;       ///< SPI card (1-based), unused for GPIO.
  static uint8_t  _step;       ///< Current index in the blink pattern.
  static uint32_t _stepStart;  ///< millis() when the current step began.
  static uint8_t  _lowPins[kMaxLow]; ///< Pins held LOW in CHARLIE_MODE.
  static uint8_t  _lowCount;         ///< Number of active entries in _lowPins.

  static void write(bool on);
};

#endif // LFX_API_SERVER_ENABLED
