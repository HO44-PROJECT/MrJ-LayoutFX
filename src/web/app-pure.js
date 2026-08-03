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
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
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
                'start_delay_ms', 'start_delay_random_ms', 'comment'];
    for (var i = 0; i < keys.length; i++) {
      if (cfgDev[keys[i]] !== undefined) d[keys[i]] = cfgDev[keys[i]];
    }
  }
  return d;
}

// Minimal HTML-escaping for text interpolated into innerHTML (see
// cfgErrorList() in app-config.js) — property names/paths come from the
// user's own uploaded JSON file, so treat them as untrusted text.
function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

// Turns Ajv's raw errors[] (see validateConfig.errors, app-config.js
// uploadConfig()) into one short, translated line per distinct instancePath.
//
// Ajv reports one error per failed oneOf branch PLUS a summary 'oneOf' error
// for the same instancePath — keeping all of them produces the unreadable
// wall of near-duplicate messages this function exists to avoid. Only the
// most specific error per path is kept: 'oneOf' summaries are skipped in
// favor of an already-seen, more precise error (e.g. additionalProperties)
// for that same path.
//
// `translate` is injected (rather than calling the global t() directly) so
// this stays a pure, dependency-free function testable under node --test —
// pass the real i18n t() from app-config.js at the call site.
function formatConfigErrors(errors, translate) {
  var byPath = {};
  errors.forEach(function (e) {
    var path = e.instancePath || '';
    if (!(path in byPath) || e.keyword !== 'oneOf') byPath[path] = e;
  });

  return Object.keys(byPath).map(function (path) {
    var e = byPath[path];
    var rootLabel = translate('cfg.err.root');
    var niceParams = {
      path: path || rootLabel,
      prop: (e.params && (e.params.additionalProperty || e.params.missingProperty)) || '',
      type: e.params && e.params.type,
      limit: e.params && e.params.limit,
      allowed: e.params && e.params.allowedValues ? e.params.allowedValues.join(', ') : ''
    };
    var key = 'cfg.err.' + e.keyword;
    var template = translate(key);
    // translate() returns the key itself when unmapped — fall back to Ajv's
    // own message rather than showing a raw, untranslated key to the user.
    if (template === key) return (path || rootLabel) + ': ' + e.message;
    return template.replace(/\{(\w+)\}/g, function (_, k) { return niceParams[k]; });
  });
}

// Export for node tests; no-op in the browser bundle (module is undefined there).
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { deDefaultStateValue, mergeDeviceForEditor, escapeHtml, formatConfigErrors };
}
