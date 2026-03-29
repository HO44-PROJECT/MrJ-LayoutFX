/**
 * @file PinState.h
 * @brief PIN_ID type router — selects the GPIO or SPI-aware variant at compile time.
 *
 * All code should include this file. Never include PinStateGPIO.h or PinStateSPI.h
 * directly.
 *
 *   SPI_CARDS undefined  →  PinStateGPIO.h  (PIN_ID = uint8_t, Nano-safe)
 *   SPI_CARDS defined    →  PinStateSPI.h   (PIN_ID = struct{pin, card}, ESP32 + 74HC595)
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#ifdef SPI_CARDS
#  include "devices/PinStateSPI.h"
#else
#  include "devices/PinStateGPIO.h"
#endif
