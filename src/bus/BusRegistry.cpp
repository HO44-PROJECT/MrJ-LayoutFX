/**
 * @file BusRegistry.cpp
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "bus/BusRegistry.h"

#ifdef MRJFX_CONFIG_ENABLED

// ── Static member definitions ─────────────────────────────────────────────────

BusRegistry::UartEntry BusRegistry::_uarts[MRJFX_BUS_MAX_UART] = {};
uint8_t BusRegistry::_uartCount = 0;

int BusRegistry::_spiMosi = -1;
int BusRegistry::_spiSclk = -1;
int BusRegistry::_spiLatch = -1;
uint8_t BusRegistry::_spiCardPinCounts[MRJFX_BUS_MAX_SPI_CARDS] = {};
uint8_t BusRegistry::_spiCardCount = 0;
bool BusRegistry::_spiReady = false;

BusRegistry::I2cEntry BusRegistry::_i2cs[MRJFX_BUS_MAX_I2C] = {};
uint8_t BusRegistry::_i2cCount = 0;
bool BusRegistry::_i2cReady = false;

int BusRegistry::_dccPin = -1;

// ── Registration ──────────────────────────────────────────────────────────────

/**
 * @brief Register a UART bus entry for later activation by activateUart().
 * @param key    Bus key string (e.g. "uart1") — must match the JSON "buses" key.
 * @param serial Pointer to the corresponding ESP32 HardwareSerial instance.
 * @param tx     GPIO number for TX.
 * @param rx     GPIO number for RX.
 * @param baud   Baud rate to use when the port is activated.
 */
void BusRegistry::regUart(const char *key, HardwareSerial *serial, int tx, int rx, int baud) {
  if (_uartCount >= MRJFX_BUS_MAX_UART) {
    LOG_PRINTLN(F("BusRegistry: MRJFX_BUS_MAX_UART reached"));
    return;
  }
  UartEntry &e = _uarts[_uartCount++];
  strncpy(e.key, key, sizeof(e.key) - 1);
  e.key[sizeof(e.key) - 1] = '\0';
  e.serial = serial;
  e.tx = tx;
  e.rx = rx;
  e.baud = baud;
  e.initialized = false;
}

/**
 * @brief Record the SPI master bus pin configuration.
 * @param mosi  GPIO for MOSI.
 * @param sclk  GPIO for SCLK.
 * @param latch GPIO for latch (RCLK on 74HC595).
 */
void BusRegistry::regSpi(int mosi, int sclk, int latch) {
  _spiMosi = mosi;
  _spiSclk = sclk;
  _spiLatch = latch;
}

/**
 * @brief Register one SPI daisy-chain slot (appends to the card list).
 * @param pinCount Number of output bits on this 74HC595 card.
 */
void BusRegistry::regSpiCard(uint8_t pinCount) {
  if (_spiCardCount >= MRJFX_BUS_MAX_SPI_CARDS) {
    LOG_PRINTLN(F("BusRegistry: max SPI cards reached"));
    return;
  }
  _spiCardPinCounts[_spiCardCount++] = pinCount;
}

/**
 * @brief Register an I2C bus entry for later activation by activateI2c().
 * @param key Bus key string.
 * @param sda GPIO number for SDA.
 * @param scl GPIO number for SCL.
 */
void BusRegistry::regI2c(const char *key, int sda, int scl) {
  if (_i2cCount >= MRJFX_BUS_MAX_I2C) {
    LOG_PRINTLN(F("BusRegistry: MRJFX_BUS_MAX_I2C reached"));
    return;
  }
  I2cEntry &e = _i2cs[_i2cCount++];
  strncpy(e.key, key, sizeof(e.key) - 1);
  e.key[sizeof(e.key) - 1] = '\0';
  e.sda = sda;
  e.scl = scl;
  e.initialized = false;
}

/**
 * @brief Record the DCC input pin number.
 * @param pin GPIO number used as DCC signal input.
 */
void BusRegistry::regDcc(int pin) {
  _dccPin = pin;
}

// ── Activation ────────────────────────────────────────────────────────────────

/**
 * @brief Initialise and return the HardwareSerial* for a UART bus key.
 *        Calls serial->begin() only on the first invocation (idempotent).
 * @param key Bus key string previously registered with regUart().
 * @return Pointer to the active HardwareSerial, or nullptr if key not found
 *         or the port configuration is incomplete.
 */
HardwareSerial *BusRegistry::activateUart(const char *key) {
  for (uint8_t i = 0; i < _uartCount; i++) {
    UartEntry &e = _uarts[i];
    if (strcmp(e.key, key) != 0)
      continue;
    if (!e.initialized) {
      if (!e.serial || e.tx < 0 || e.rx < 0) {
        LOG_PRINT(F("BusRegistry: uart incomplete config for "));
        LOG_PRINTLN(key);
        return nullptr;
      }
      e.serial->begin(e.baud, SERIAL_8N1, e.rx, e.tx);
      e.initialized = true;
      LOG_PRINT(F("BusRegistry: uart activated "));
      LOG_PRINT(key);
      LOG_PRINT(F(" tx="));
      LOG_PRINT(e.tx);
      LOG_PRINT(F(" rx="));
      LOG_PRINT(e.rx);
      LOG_PRINT(F(" baud="));
      LOG_PRINTLN(e.baud);
    }
    return e.serial;
  }
  LOG_PRINT(F("BusRegistry: uart key not found — "));
  LOG_PRINTLN(key);
  return nullptr;
}

/**
 * @brief Initialise Spi595Bus with the registered pins and card list.
 *        Idempotent — hardware is touched only on the first call.
 * @return true if SPI is ready after the call, false if not configured.
 */
bool BusRegistry::activateSpi() {
  if (_spiReady)
    return true;
  #ifdef MRJFX_SPI_CARDS_ENABLED
  if (_spiMosi < 0 || _spiSclk < 0 || _spiLatch < 0 || _spiCardCount == 0)
    return false;
  Spi595Bus::init(_spiMosi, _spiSclk, _spiLatch, _spiCardPinCounts, _spiCardCount);
  _spiReady = true;
  LOG_PRINT(F("BusRegistry: SPI activated mosi="));
  LOG_PRINT(_spiMosi);
  LOG_PRINT(F(" sclk="));
  LOG_PRINT(_spiSclk);
  LOG_PRINT(F(" latch="));
  LOG_PRINT(_spiLatch);
  LOG_PRINT(F(" cards="));
  LOG_PRINTLN(_spiCardCount);
  #endif
  return _spiReady;
}

/**
 * @brief Initialise Wire for the first registered I2C bus.
 *        Idempotent — Wire.begin() is called only once.
 * @return Pointer to the active TwoWire instance, or nullptr if none registered.
 */
TwoWire *BusRegistry::activateI2c() {
  if (_i2cReady)
    return &Wire;
  if (_i2cCount == 0)
    return nullptr;
  I2cEntry &e = _i2cs[0];
  if (!e.initialized) {
    Wire.begin(e.sda, e.scl);
    e.initialized = true;
    _i2cReady = true;
    LOG_PRINT(F("BusRegistry: I2C activated sda="));
    LOG_PRINT(e.sda);
    LOG_PRINT(F(" scl="));
    LOG_PRINTLN(e.scl);
  }
  return &Wire;
}

// ── Loop / reset ─────────────────────────────────────────────────────────────

/** @brief Forward the SPI output image to the 74HC595 chain. No-op if SPI is not active. */
void BusRegistry::flush() {
  #ifdef MRJFX_SPI_CARDS_ENABLED
  if (_spiReady)
    Spi595Bus::flush();
  #endif
}

/**
 * @brief Clear all registration state so load() can re-register buses and devices.
 *        Does NOT reset _i2cReady / _spiReady — the physical bus hardware (Wire,
 *        SPI) is already running and must not be re-initialised on hot-reload.
 *        Re-calling Wire.begin() on ESP32 can corrupt the I2C peripheral.
 */
void BusRegistry::reset() {
  _uartCount = 0;
  _spiMosi = -1;
  _spiSclk = -1;
  _spiLatch = -1;
  _spiCardCount = 0;
  // _spiReady intentionally preserved — SPI hardware stays configured.
  _i2cCount = 0;
  // _i2cReady intentionally preserved — Wire must not be re-initialised.
  _dccPin = -1;
  for (uint8_t i = 0; i < MRJFX_BUS_MAX_UART; i++)
    _uarts[i] = {};
  for (uint8_t i = 0; i < MRJFX_BUS_MAX_I2C; i++)
    _i2cs[i] = {};
  for (uint8_t i = 0; i < MRJFX_BUS_MAX_SPI_CARDS; i++)
    _spiCardPinCounts[i] = 0;
}

#endif // MRJFX_CONFIG_ENABLED
