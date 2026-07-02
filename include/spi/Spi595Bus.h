/**
 * @file Spi595Bus.h
 *
 * @brief Library-level driver for a daisy-chain of 74HC595 shift registers on VSPI.
 *
 * @objective Provide a singleton write-only SPI bus usable by any Device coroutine.
 *            Maintains an in-memory image of all card outputs; setPin() updates the
 *            image and marks it dirty, and flush() (called once per loop iteration
 *            from MrJFX::loop) pushes it to hardware in a single SPI.transfer burst
 *            only when it actually changed.
 *
 * Daisy-chain byte order (MSBFIRST hardware):
 *   • Card 1 is closest to MOSI (first in the JSON spi_cards array).
 *   • SPI shifts the last card's bytes first; card 1's bytes arrive last in the chain.
 *   • Buffer layout: buf[0] = MSByte of last card … buf[totalBytes-1] = LSByte of card 1.
 *   • Within each card, bit index equals the JSON "wiring" value:
 *     wiring 0 → Q0 of first HC595 chip on that card (LSByte, bit 0).
 *
 * Usage (called by DeviceFactory::load once the JSON is parsed):
 *   uint8_t counts[n] = { card1.pin_count, card2.pin_count, ... };
 *   Spi595Bus::init(mosi, sclk, latch, counts, n);
 *
 * Per-tick usage (called by Device::pin_it / outputActive / simulatePWM_spi):
 *   Spi595Bus::setPin(card1based, bit, HIGH);  // updates image + marks it dirty
 *
 * @note Only compiled when MRJFX_SPI_CARDS_ENABLED is defined.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_SPI_CARDS_ENABLED

#include <Arduino.h>
#include <SPI.h>
#include "devices/PinStateSPI.h"

class Spi595Bus {
public:

  static constexpr uint8_t MAX_CARDS     = 8;
  static constexpr uint8_t MAX_BYTES     = MAX_CARDS * 4;  ///< 8 cards × max 32 bits / 8
  static constexpr uint32_t CLOCK_HZ    = 10000000;        ///< 10 MHz — same as poc_k2000_SPI

  /**
   * @brief Initialise the VSPI bus, latch pin, and output image.
   *
   * @param mosi          GPIO for MOSI (data to first register DS pin).
   * @param sclk          GPIO for SCLK (shared SH_CP).
   * @param latch         GPIO for latch (shared ST_CP).
   * @param cardPinCounts Array of pin_count values, one per card (must be multiple of 8).
   * @param cardCount     Number of entries in cardPinCounts (1-based index in setPin).
   */
  static void init(int mosi, int sclk, int latch,
                   const uint8_t* cardPinCounts, uint8_t cardCount);

  /**
   * @brief Set one output bit and flush the full chain to hardware.
   *
   * @param card1based  Card index (1-based, matching JSON "board" field).
   * @param bit         Bit index within the card (0-based, matching JSON "wiring").
   * @param value       HIGH (1) or LOW (0).
   */
  static void setPin(uint8_t card1based, uint8_t bit, uint8_t value);

  /**
   * @brief Push the current image to hardware without changing any bit.
   *
   * No-op when the image is unchanged since the last flush (dirty flag). Called
   * every coroutine step from MrJFX::loop, so skipping the SPI transaction while
   * the image is static is what keeps GPIO software-PWM effects jitter-free when
   * an SPI bus is configured (backlog #48).
   */
  static void flush();

  /**
   * @brief Flush a temporary image with only one bit set — does NOT modify _buf.
   *
   * Used by the debug test endpoint: shows only the requested LED without
   * disturbing the shared animation buffer.  Device coroutines restore normal
   * states on their next setPin() call.
   *
   * @param card1based  Card index (1-based).
   * @param bit         Wiring value (1-based, same convention as setPin).
   * @param value       HIGH (1) or LOW (0).
   */
  static void testPin(uint8_t card1based, uint8_t bit, uint8_t value);

  /** @brief True if init() has been called. */
  static bool ready() { return _totalBytes > 0; }

private:

  static int     _latch;
  static uint8_t _totalBytes;
  static uint8_t _cardCount;
  static uint8_t _cardBitOffset[MAX_CARDS]; ///< Global bit offset of card[i] (0-based array).
  static uint8_t _cardPinCount [MAX_CARDS]; ///< Pin count for card[i].
  static uint8_t _buf          [MAX_BYTES]; ///< Output image (SPI transfer order).
  static bool    _dirty;                    ///< _buf changed since last flush(); gates the SPI transaction.
};

/**
 * @brief Write HIGH or LOW to a PIN_ID — SPI mode routing.
 *
 * Routes to Spi595Bus::setPin for SPI daughter-card bits, or to digitalWrite
 * for native GPIO pins. Replaces direct digitalWrite calls in simulatePWM and
 * other pin-write sites so all led_fx effects compile unchanged in SPI mode.
 *
 * Defined here (after the full Spi595Bus class) so it is available wherever
 * Spi595Bus.h is included (Device.h, Pov.h and transitively all led_fx units).
 */
inline void pinWrite(PIN_ID p, uint8_t value) {
  if (p.isSpi()) { Spi595Bus::setPin(p.card, p.pin, value); return; }
  digitalWrite(p.pin, value);
}

#else

#error "Spi595Bus.h included but MRJFX_SPI_CARDS_ENABLED is not defined"

#endif // MRJFX_SPI_CARDS_ENABLED
