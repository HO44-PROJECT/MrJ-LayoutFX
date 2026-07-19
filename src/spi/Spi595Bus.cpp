/**
 * @file Spi595Bus.cpp
 * @project MrJ-LayoutFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include <LayoutFX_define.h>

#ifdef LFX_SPI_CARDS_ENABLED

  #include "spi/Spi595Bus.h"
  #include "utils/utils.h"

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

int Spi595Bus::_latch = -1;
uint8_t Spi595Bus::_totalBytes = 0;
uint8_t Spi595Bus::_cardCount = 0;
uint8_t Spi595Bus::_cardBitOffset[Spi595Bus::MAX_CARDS] = {};
uint8_t Spi595Bus::_cardPinCount[Spi595Bus::MAX_CARDS] = {};
uint8_t Spi595Bus::_buf[Spi595Bus::MAX_BYTES] = {};
bool Spi595Bus::_dirty = false;

// ---------------------------------------------------------------------------

void Spi595Bus::init(int mosi, int sclk, int latch,
                     const uint8_t *cardPinCounts, uint8_t cardCount) {
  _latch = latch;

  pinMode(latch, OUTPUT);
  digitalWrite(latch, HIGH);

  // MISO unused (-1); SS managed via latch pin manually.
  SPI.begin(sclk, -1, mosi, -1);

  // Reclaim the default VSPI MISO pin (GPIO 19) as a regular GPIO.
  pinMode(19, OUTPUT);
  digitalWrite(19, LOW);

  resize(cardPinCounts, cardCount);

  LOG_PRINT(F("Spi595Bus: init ok — totalBytes="));
  LOG_PRINT(_totalBytes);
  LOG_PRINT(F(" cards="));
  LOG_PRINTLN(_cardCount);
}

// ---------------------------------------------------------------------------

void Spi595Bus::resize(const uint8_t *cardPinCounts, uint8_t cardCount) {
  _cardCount = (cardCount < MAX_CARDS) ? cardCount : MAX_CARDS;
  _totalBytes = 0;

  for (uint8_t i = 0; i < _cardCount; i++) {
    _cardBitOffset[i] = _totalBytes * 8;
    _cardPinCount[i] = cardPinCounts[i];
    _totalBytes += cardPinCounts[i] / 8;
  }

  memset(_buf, 0, sizeof(_buf));

  _dirty = true; // force the next flush() to repaint hardware with the new sizing
  flush();

  LOG_PRINT(F("Spi595Bus: resize — totalBytes="));
  LOG_PRINT(_totalBytes);
  LOG_PRINT(F(" cards="));
  LOG_PRINTLN(_cardCount);
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
  //   LOG_PRINTLN(byteIdx);

  uint8_t before = _buf[byteIdx];
  if (value) {
    _buf[byteIdx] |= (1u << bitIdx);
  } else {
    _buf[byteIdx] &= ~(1u << bitIdx);
  }
  // Only mark dirty on a real change so flush() stays a no-op while the SPI
  // image is static — otherwise the per-loop flush() in LayoutFX::loop() would run
  // a full SPI transaction on every coroutine step and jitter the software-PWM
  // timing of GPIO effects (see backlog #48).
  if (_buf[byteIdx] != before)
    _dirty = true;
}

// ---------------------------------------------------------------------------

void Spi595Bus::flush() {
  // LayoutFX::loop() calls this after every coroutine step. Skip the SPI transaction
  // entirely when nothing changed since the last flush: GPIO effects never touch
  // _buf, so during a pure-GPIO fade the image stays clean and this stays a cheap
  // boolean check — the scheduler round-trip stays tight and software PWM is smooth.
  if (!ready() || !_dirty)
    return;
  static const SPISettings settings(CLOCK_HZ, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
  digitalWrite(_latch, LOW);
  for (uint8_t i = 0; i < _totalBytes; i++) {
    SPI.transfer(_buf[i]);
  }
  digitalWrite(_latch, HIGH);
  SPI.endTransaction();
  _dirty = false;
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
  // The temp image is on the wires but _buf is unchanged; force the next flush()
  // to repaint the real image so coroutine states are restored.
  _dirty = true;
}

#endif // LFX_SPI_CARDS_ENABLED
