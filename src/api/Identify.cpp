/**
 * @file Identify.cpp
 * @brief Implementation of the LED identify blinker. See Identify.h.
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */
#include "api/Identify.h"

#ifdef LFX_API_SERVER_ENABLED

  #ifdef LFX_SPI_CARDS_ENABLED
    #include "spi/Spi595Bus.h"
  #endif

// Distinctive pattern: three short flashes then a longer pause, repeating.
// (on?, duration_ms) per step.
namespace {
struct Step { bool on; uint16_t ms; };
const Step PATTERN[] = {
    {true, 130}, {false, 130}, {true, 130}, {false, 130}, {true, 130}, {false, 750}};
const uint8_t PATTERN_LEN = sizeof(PATTERN) / sizeof(PATTERN[0]);
} // namespace

Identify::Mode Identify::_mode = Identify::NONE;
uint8_t  Identify::_pin       = 0;
uint8_t  Identify::_card      = 0;
uint8_t  Identify::_step      = 0;
uint32_t Identify::_stepStart = 0;
uint8_t  Identify::_lowPins[Identify::kMaxLow] = {0};
uint8_t  Identify::_lowCount  = 0;

void Identify::write(bool on) {
  if (_mode == GPIO_MODE) {
    digitalWrite(_pin, on ? HIGH : LOW);
  } else if (_mode == CHARLIE_MODE) {
    // Hold the other wires LOW (re-asserted each step) so current sinks through
    // the tested LED; blink the tested pin HIGH/LOW.
    for (uint8_t i = 0; i < _lowCount; i++)
      digitalWrite(_lowPins[i], LOW);
    digitalWrite(_pin, on ? HIGH : LOW);
  }
  #ifdef LFX_SPI_CARDS_ENABLED
  else if (_mode == SPI_MODE) {
    // Bit is propagated to the shift register by BusRegistry::flush() in LayoutFX::loop().
    Spi595Bus::setPin(_card, _pin, on ? 1 : 0);
  }
  #endif
}

void Identify::startGpio(uint8_t pin) {
  stop(); // drive any previous target inactive first (also sets _mode = NONE)
  pinMode(pin, OUTPUT);
  // Publish the target state BEFORE re-enabling _mode. loop() runs on Core 1 and
  // uses _mode as its guard; if _mode were set first, Core 1 could fire a write on
  // the STALE _pin (the previous target) during the window before _pin is updated,
  // stranding the previous LED HIGH. Set _mode last so Core 1 only acts on a
  // consistent (_pin, _step, _stepStart).
  _pin  = pin;
  _step = 0;
  _stepStart = millis();
  _mode = GPIO_MODE;
  write(PATTERN[0].on);
}

void Identify::startCharlieplex(uint8_t testPin, const uint8_t *lowPins, uint8_t lowCount) {
  stop();
  _lowCount = (lowCount > kMaxLow) ? kMaxLow : lowCount;
  for (uint8_t i = 0; i < _lowCount; i++) {
    _lowPins[i] = lowPins[i];
    pinMode(_lowPins[i], OUTPUT);
    digitalWrite(_lowPins[i], LOW);
  }
  pinMode(testPin, OUTPUT);
  _pin  = testPin;
  _step = 0;
  _stepStart = millis();
  _mode = CHARLIE_MODE; // publish last (see startGpio for the Core 0/1 race rationale)
  write(PATTERN[0].on);
}

  #ifdef LFX_SPI_CARDS_ENABLED
void Identify::startSpi(uint8_t card, uint8_t channel) {
  stop();
  _card = card;
  _pin  = channel;
  _step = 0;
  _stepStart = millis();
  _mode = SPI_MODE;   // publish last (see startGpio for the Core 0/1 race rationale)
  write(PATTERN[0].on);
}
  #endif

void Identify::stop() {
  if (_mode != NONE)
    write(false);
  _mode = NONE;
}

void Identify::loop() {
  if (_mode == NONE)
    return;
  if (millis() - _stepStart >= PATTERN[_step].ms) {
    _step = (uint8_t)((_step + 1) % PATTERN_LEN);
    _stepStart = millis();
    write(PATTERN[_step].on);
  }
}

#endif // LFX_API_SERVER_ENABLED
