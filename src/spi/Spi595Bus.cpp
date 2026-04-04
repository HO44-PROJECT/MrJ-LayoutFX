/**
 * @file Spi595Bus.cpp
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_SPI_CARDS_ENABLED

  #include "spi/Spi595Bus.h"

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

int Spi595Bus::_latch = -1;
uint8_t Spi595Bus::_totalBytes = 0;
uint8_t Spi595Bus::_cardCount = 0;
uint8_t Spi595Bus::_cardBitOffset[Spi595Bus::MAX_CARDS] = {};
uint8_t Spi595Bus::_cardPinCount[Spi595Bus::MAX_CARDS] = {};
uint8_t Spi595Bus::_buf[Spi595Bus::MAX_BYTES] = {};

// ---------------------------------------------------------------------------

void Spi595Bus::init(int mosi, int sclk, int latch,
                     const uint8_t *cardPinCounts, uint8_t cardCount) {
  _latch = latch;
  _cardCount = (cardCount < MAX_CARDS) ? cardCount : MAX_CARDS;
  _totalBytes = 0;

  for (uint8_t i = 0; i < _cardCount; i++) {
    _cardBitOffset[i] = _totalBytes * 8;
    _cardPinCount[i] = cardPinCounts[i];
    _totalBytes += cardPinCounts[i] / 8;
  }

  memset(_buf, 0, sizeof(_buf));

  pinMode(latch, OUTPUT);
  digitalWrite(latch, HIGH);

  // MISO unused (-1); SS managed via latch pin manually.
  SPI.begin(sclk, -1, mosi, -1);

  // Reclaim the default VSPI MISO pin (GPIO 19) as a regular GPIO.
  pinMode(19, OUTPUT);
  digitalWrite(19, LOW);

  flush(); // All outputs LOW at startup.

  Serial.print(F("Spi595Bus: init ok — totalBytes="));
  Serial.print(_totalBytes);
  Serial.print(F(" cards="));
  Serial.println(_cardCount);
}

// ---------------------------------------------------------------------------

void Spi595Bus::setPin(uint8_t card1based, uint8_t bit, uint8_t value) {
  if (!ready())
    return;
  if (card1based == 0 || card1based > _cardCount)
    return;

  uint8_t idx = card1based - 1;
  uint8_t bit0 = (uint8_t)(bit - 1u); // wiring is 1-based everywhere; convert here to 0-based
  if (bit == 0 || bit0 >= _cardPinCount[idx])
    return;

  // Global bit index (card 1 starts at offset 0).
  uint8_t globalBit = _cardBitOffset[idx] + bit0;

  // Reverse byte order: card 1 LSByte is at buf[totalBytes-1].
  uint8_t byteIdx = _totalBytes - 1 - (globalBit / 8);
  uint8_t bitIdx = globalBit % 8;

  // if (byteIdx != 0 and byteIdx != 1)
  //   Serial.println(byteIdx);

  if (value) {
    _buf[byteIdx] |= (1u << bitIdx);
  } else {
    _buf[byteIdx] &= ~(1u << bitIdx);
  }
}

// ---------------------------------------------------------------------------

void Spi595Bus::flush() {
  static const SPISettings settings(CLOCK_HZ, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
  digitalWrite(_latch, LOW);
  for (uint8_t i = 0; i < _totalBytes; i++) {
    SPI.transfer(_buf[i]);
  }
  digitalWrite(_latch, HIGH);
  SPI.endTransaction();
}

// ---------------------------------------------------------------------------

void Spi595Bus::testPin(uint8_t card1based, uint8_t bit, uint8_t value) {
  if (!ready())
    return;
  if (card1based == 0 || card1based > _cardCount)
    return;

  uint8_t idx = card1based - 1;
  uint8_t bit0 = (uint8_t)(bit - 1u); // 1-based → 0-based
  if (bit == 0 || bit0 >= _cardPinCount[idx])
    return;

  // Build a fresh zero buffer; set only the tested bit.
  uint8_t tmp[MAX_BYTES] = {};
  uint8_t globalBit = _cardBitOffset[idx] + bit0;
  uint8_t byteIdx = _totalBytes - 1 - (globalBit / 8);
  uint8_t bitIdx = globalBit % 8;
  if (value)
    tmp[byteIdx] |= (1u << bitIdx);

  // Send the temporary buffer — _buf is NOT modified.
  static const SPISettings settings(CLOCK_HZ, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
  digitalWrite(_latch, LOW);
  for (uint8_t i = 0; i < _totalBytes; i++) {
    SPI.transfer(tmp[i]);
  }
  digitalWrite(_latch, HIGH);
  SPI.endTransaction();
}

#endif // MRJFX_SPI_CARDS_ENABLED
