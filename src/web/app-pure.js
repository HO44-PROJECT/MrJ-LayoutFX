/**
 * @file app-pure.js
 * @brief Pure, DOM-free helpers for the WebUI — the bits worth unit-testing.
 *
 * These functions take plain data and return plain data (no DOM, no fetch, no
 * globals), so they can be exercised by node tests under test/web/ AND bundled
 * into the WebUI like any other module. The CommonJS guard at the bottom exports
 * them for the tests; in the browser bundle `module` is undefined and the guard
 * is a no-op.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

// Value for the device editor's "default state" dropdown.
//
// MUST derive from the PERSISTED config field (default_state), never from the
// runtime current state (desired). Deriving it from `desired` is what silently
// baked default_state:"on" into configs whenever a device was edited while
// running (e.g. a tested motor) — the 2026-06 "servo starts on its own" bug.
function deDefaultStateValue(dev) {
  var ds = dev && dev.default_state;
  if (ds === undefined || ds === '' || ds === 'off') return '';
  if (ds === 'on') return 'on';
  return String(ds); // numeric state value (signals/servos)
}

// Build the editor's working device object from a runtime device (/api/devices)
// overlaid with its persisted config entry. The config is the source of truth
// for servo/motor parameters and for default_state, so re-saving a device never
// drops them.
function mergeDeviceForEditor(rtDev, cfgDev) {
  var d = {
    id: rtDev.id, type: rtDev.type, board: rtDev.board,
    desired: rtDev.desired, state: rtDev.state, addr: rtDev.addr,
    pins: rtDev.pins, label: rtDev.label
  };
  if (cfgDev) {
    var keys = ['angle_a', 'angle_b', 'positions', 'pulse_min_us', 'pulse_max_us',
                'speed', 'states', 'neutral_us', 'default_state',
                'start_delay_ms', 'start_delay_random_ms'];
    for (var i = 0; i < keys.length; i++) {
      if (cfgDev[keys[i]] !== undefined) d[keys[i]] = cfgDev[keys[i]];
    }
  }
  return d;
}

// Export for node tests; no-op in the browser bundle (module is undefined there).
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { deDefaultStateValue, mergeDeviceForEditor };
}
