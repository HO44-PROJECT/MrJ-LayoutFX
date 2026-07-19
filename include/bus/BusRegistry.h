/**
 * @file BusRegistry.h
 * @brief Centralises hardware bus lifecycle management (UART, SPI, I2C, DCC).
 *
 * Two-phase API:
 *   reg*()      — record bus configuration without touching hardware (config parse).
 *   activate*() — initialise on first device use, idempotent.
 *
 * Additional helpers:
 *   flush()     — propagate the SPI image to hardware (call from LayoutFX::loop).
 *   reset()     — clear all internal state (tests only, no hardware interaction).
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author  MrJ
 * @date    2026-04-22
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_CONFIG_ENABLED

  #include "utils/utils.h"
  #include <Arduino.h>
  #include <Wire.h>

  #ifdef LFX_SPI_CARDS_ENABLED
    #include "spi/Spi595Bus.h"
  #endif

class BusRegistry {
public:
  // ── Registration (config parse phase) ─────────────────────────────────────

  /**
   * @brief Record a UART bus configuration for later activation.
   * @param key    Bus key string (e.g. "uart1").
   * @param serial Matching ESP32 HardwareSerial instance (Serial / Serial1 / Serial2).
   * @param tx     GPIO pin for TX.
   * @param rx     GPIO pin for RX.
   * @param baud   Baud rate.
   */
  static void regUart(const char *key, HardwareSerial *serial, int tx, int rx, int baud);

  /**
   * @brief Record the SPI master bus pins.
   * @param mosi  GPIO pin for MOSI.
   * @param sclk  GPIO pin for SCLK.
   * @param latch GPIO pin for latch (RCLK).
   */
  static void regSpi(int mosi, int sclk, int latch);

  /**
   * @brief Register one SPI daisy-chain slot.
   * @param pinCount Number of output bits on this card.
   */
  static void regSpiCard(uint8_t pinCount);

  /**
   * @brief Record an I2C bus configuration for later activation.
   * @param key Bus key string.
   * @param sda GPIO pin for SDA.
   * @param scl GPIO pin for SCL.
   */
  static void regI2c(const char *key, int sda, int scl);

  /**
   * @brief Record the DCC input pin.
   * @param pin GPIO pin number.
   */
  static void regDcc(int pin);

  // ── Activation (first device use) ─────────────────────────────────────────

  /**
   * @brief Initialise and return the HardwareSerial* for a UART bus key.
   *        Idempotent — calls serial->begin() only on the first invocation.
   * @param key Bus key string registered with regUart().
   * @return Pointer to the activated HardwareSerial, or nullptr on error.
   */
  static HardwareSerial *activateUart(const char *key);

  /**
   * @brief Initialise Spi595Bus if not already done.
   * @return true if SPI is ready after the call.
   */
  static bool activateSpi();

  /**
   * @brief Initialise Wire for the first registered I2C bus.
   * @return Pointer to the activated TwoWire instance, or nullptr if none registered.
   */
  static TwoWire *activateI2c();

  // ── Loop helpers ───────────────────────────────────────────────────────────

  static void flush(); ///< Forward to Spi595Bus::flush() if SPI is active.
  static void reset(); ///< Clear all internal state (no hardware interaction).

  // ── Accessors ─────────────────────────────────────────────────────────────

  static int dccPin() { return _dccPin; }
  static bool spiReady() { return _spiReady; }
  static bool i2cReady() { return _i2cReady; }

  /**
   * @brief Mark the I2C bus as already initialised (Wire.begin called externally).
   *        Prevents activateI2c() from calling Wire.begin() a second time, which
   *        corrupts the I2C peripheral on arduino-esp32 v3.x.
   *        Must be called in LayoutFX::init() right after Wire.begin().
   */
  static void preInitI2c() { _i2cReady = true; }

private:
  static constexpr uint8_t BUS_KEY_LEN = 32; ///< Max length for a bus key string (incl. NUL).

  // UART
  struct UartEntry {
    char key[BUS_KEY_LEN];
    HardwareSerial *serial;
    int tx, rx, baud;
    bool initialized;
  };
  static UartEntry _uarts[LFX_BUS_MAX_UART]; ///< Override limit via BUS_MAX_UART in config.h.
  static uint8_t _uartCount;

  // SPI
  static int _spiMosi, _spiSclk, _spiLatch;
  static uint8_t _spiCardPinCounts[LFX_BUS_MAX_SPI_CARDS]; ///< Override limit via BUS_MAX_SPI_CARDS in config.h.
  static uint8_t _spiCardCount;
  static bool _spiReady;

  // I2C
  struct I2cEntry {
    char key[BUS_KEY_LEN];
    int sda, scl;
    bool initialized;
  };
  static I2cEntry _i2cs[LFX_BUS_MAX_I2C]; ///< Override limit via BUS_MAX_I2C in config.h.
  static uint8_t _i2cCount;
  static bool _i2cReady;

  // DCC
  static int _dccPin;
};

#endif // LFX_CONFIG_ENABLED
