/**
 * @file SafeMode.h
 * @brief Double-reset → recovery "safe mode": bypass config + force SoftAP.
 *
 * Counts boots in NVS within a short window. Two quick resets / power-cycles
 * (bulb-style, survives a power-off) latch safe mode for the next session:
 * ConfigManager is skipped (no devices loaded, GPIO/UART left untouched) and
 * WiFi comes up as the known SoftAP, so the WebUI is always reachable to repair
 * a config that crashes or makes the device unreachable.
 *
 * Non-persistent: the next normal boot loads the config again. The config file
 * itself is never touched — the user fixes/deletes it from the WebUI.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */
#pragma once

#include <Arduino.h>

class SafeMode {
public:
  /// Call once, very early in init(): bump the NVS boot counter and latch
  /// active() when the quick-reset threshold is reached. No-op (stays inactive)
  /// if NVS is unavailable.
  static void begin();

  /// True when this boot is a recovery session (skip config, force SoftAP).
  static bool active() { return _active; }

  /// Call from loop(): once past the detection window, clear the counter so a
  /// normal (single) boot never accumulates toward the threshold.
  static void loop();

private:
  static bool _active;
  static bool _windowClosed;
};
