/**
 * @file SafeMode.cpp
 * @brief NVS-backed double-reset detector — see SafeMode.h.
 *
 * @project MrJ-LayoutFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "utils/SafeMode.h"

// NVS (Preferences) is ESP32-only; on AVR this compiles to nothing and the
// methods are never referenced (safe mode is gated by LFX_CONFIG_ENABLED).
#ifdef ESP32

#include <Preferences.h>

namespace {
constexpr char     kNvsNamespace[] = "mrjfx";  ///< NVS namespace.
constexpr char     kNvsKey[]       = "rstcnt"; ///< Boot-counter key.
constexpr uint8_t  kThreshold      = 2;        ///< Quick resets to enter safe mode.
constexpr uint32_t kWindowMs       = 4000;     ///< Counter is cleared after this many ms of normal run.
} // namespace

bool SafeMode::_active = false;
bool SafeMode::_windowClosed = false;

void SafeMode::begin() {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false))
    return; // NVS unavailable → never latch safe mode

  uint8_t n = prefs.getUChar(kNvsKey, 0) + 1;
  if (n >= kThreshold) {
    _active = true;
    prefs.putUChar(kNvsKey, 0); // consume now so the *next* boot is normal again
    _windowClosed = true;       // nothing left for loop() to clear
  } else {
    prefs.putUChar(kNvsKey, n);
  }
  prefs.end();
}

void SafeMode::loop() {
  if (_windowClosed) return;
  if (millis() < kWindowMs) return; // still inside the multi-tap window
  _windowClosed = true;

  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false)) return;
  prefs.putUChar(kNvsKey, 0);
  prefs.end();
}

#else // !ESP32 — provide empty symbols so any stray reference still links.

bool SafeMode::_active = false;
bool SafeMode::_windowClosed = false;
void SafeMode::begin() {}
void SafeMode::loop() {}

#endif
