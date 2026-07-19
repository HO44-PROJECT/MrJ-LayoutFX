/**
 * @file PinStateSPI.h
 * @brief PIN_ID as a two-byte struct — for ESP32 with SPI daughter-card expansion.
 *
 * Do not include directly. Include devices/PinState.h which routes to this file
 * or to PinStateGPIO.h depending on the LFX_SPI_CARDS_ENABLED build flag.
 *
 * PIN_ID encodes either a native GPIO pin or a bit on a 74HC595 daughter card:
 *
 *   PIN_ID::gpio(pin)       — native ESP32 GPIO, card = 0
 *   PIN_ID::spi(card, bit)  — bit 0–15 on daughter card 1–N
 *
 * Comparison with NO_PIN and between PIN_IDs uses operator== / operator!=,
 * so all existing  if (p == NO_PIN) / if (p != NO_PIN)  sites compile unchanged.
 *
 * @project MrJ-LayoutFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <Arduino.h>
#include "utils/ArduinoBoard.h"

#define PIN_NO_MODE  255
#define PIN_NO_VALUE 255

/**
 * @struct PIN_STATE
 * @brief Mode and initial value of a pin (INPUT/OUTPUT + LOW/HIGH).
 *        Identical on all platforms — only PIN_ID differs.
 */
typedef struct {
  char    id;       ///< Unique character tag (e.g. 'L', 'H', 'Z', 'P').
  uint8_t mode;     ///< Pin mode  (INPUT, OUTPUT, INPUT_PULLUP).
  uint8_t value;    ///< Pin value (LOW, HIGH).
} PIN_STATE;

bool operator==(const PIN_STATE &x, const PIN_STATE &y);

extern const PIN_STATE Z;   ///< INPUT, no pull-up.
extern const PIN_STATE P;   ///< INPUT_PULLUP.
extern const PIN_STATE L;   ///< OUTPUT LOW.
extern const PIN_STATE H;   ///< OUTPUT HIGH.
extern const PIN_STATE I;   ///< Ignore (no mode, no value).
extern const PIN_STATE SH;  ///< Stay HIGH.

/**
 * @struct PIN_ID
 * @brief Two-byte opaque pin reference — GPIO or SPI daughter-card bit.
 *
 * Use the static factory methods to construct:
 *   PIN_ID p = PIN_ID::gpio(17);       // native GPIO 17
 *   PIN_ID p = PIN_ID::spi(1, 3);      // bit 3 on daughter card 1
 */
struct PIN_ID {
    uint8_t pin;   ///< GPIO pin number  OR  bit index on SPI daughter card (0–15).
    uint8_t card;  ///< 0 = native GPIO,  1–N = SPI daughter card index.

    /** @brief Construct a native GPIO reference. */
    static PIN_ID gpio(uint8_t pin)             { return {pin, 0}; }

    /** @brief Construct a SPI daughter-card bit reference. */
    static PIN_ID spi(uint8_t card, uint8_t bit){ return {bit, card}; }

    /** @brief True if this refers to a native GPIO pin. */
    bool isGpio() const { return card == 0; }

    /** @brief True if this refers to a SPI daughter-card bit. */
    bool isSpi()  const { return card > 0; }

    bool operator==(const PIN_ID& o) const { return pin == o.pin && card == o.card; }
    bool operator!=(const PIN_ID& o) const { return !(*this == o); }
};

/** @brief Sentinel for an unassigned or invalid pin. */
static constexpr PIN_ID NO_PIN = {255, 0};

/** @brief True if p is a valid (assigned) pin reference. */
#define VALID_PIN(p) ((p) != NO_PIN)

/**
 * @brief Return a uint8_t OLED display ID from a PIN_ID.
 *
 * SPI pins: card * 32 + bit (ensures unique IDs across all cards).
 * GPIO pins: pin number directly.
 */
inline uint8_t pinId(PIN_ID p) {
  return p.isSpi() ? (uint8_t)(p.card * 32 + p.pin) : p.pin;
}
