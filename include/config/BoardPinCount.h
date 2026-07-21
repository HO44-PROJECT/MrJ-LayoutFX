/**
 * @file BoardPinCount.h
 * @brief Compact board-type → output-pin-count entry (firmware-side).
 *
 * The full board_types catalog lives gzipped in PROGMEM (embedded_board_types.h)
 * and is served to the WebUI.  The firmware itself only needs the number of
 * output pins per SPI board type, to size 74HC595 daisy-chain cards when a board
 * entry omits an explicit "pin_count".  A tiny uncompressed table of these counts
 * is generated alongside the gzip catalog (embedded_board_pincounts.h) and passed
 * to DeviceFactory::load().
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <Arduino.h>

/** @brief One board-type → output-pin-count entry. */
struct BtPinCount {
  const char *type; ///< Board type name (matches a board entry "type" string).
  uint8_t pins;     ///< Number of pins carrying a "wiring" field in board_types.json.
};
