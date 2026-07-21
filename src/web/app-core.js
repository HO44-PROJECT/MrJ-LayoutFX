/**
 * @file app-core.js
 * @brief Cockpit view: globals, navigation, device card renderers, filter, grid, poll.
 *
 * Core WebUI module bundled with other app-*.js modules by build_webui.py.
 * Manages device cards, real-time polling, and main navigation.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

/* ── Constants ──────────────────────────────────────────────────────── */
var POLL = 3000; // cockpit poll interval in ms; user-adjustable, persisted in localStorage
// Category lists derived from _deviceTypes after fetch.
// Kept as globals so inline onclick="...SERVO_TYPES..." handlers still work.
var SERVO_TYPES = [];
var I2C_SERVO_TYPES = [];
var I2C_MOTOR_TYPES = [];
var STATIC_TYPES = [];
var TRAFFIC_TYPES = [];
var SERVO_STATES = [];

/* ── Navigation ─────────────────────────────────────────────────────── */

var _currentView = 'cockpit';

// Toggle the side navigation drawer open/closed.
function toggleDrawer() {
  document.body.classList.toggle('drawer-open');
}

// Close the side navigation drawer.
function closeDrawer() {
  document.body.classList.remove('drawer-open');
}

// Navigate directly to the boards & extensions tab in the config view.
// Used by the empty-cockpit CTA and post-wizard navigation.
function goToBoards() {
  switchView('config');
  switchCfgTab('boards');
}

// Activate a named view (cockpit | config | about).
// 'params' is a legacy alias that redirects to 'about'.
// Hides/shows the status bar (cockpit-only).
function switchView(name) {
  if (name === 'params') name = 'about'; // params merged into about
  // Hide all views
  document.querySelectorAll('.view').forEach(function (v) {
    v.classList.remove('active');
  });
  // Show target view
  document.getElementById('view-' + name).classList.add('active');

  // Update nav items
  document.querySelectorAll('.nav-item').forEach(function (item) {
    item.classList.toggle('active', item.dataset.view === name);
  });

  // Show status bar only on cockpit (All on/off now lives in #ck-toolbar
  // itself, #135 — that toolbar is only ever shown for the cockpit view, so
  // it no longer needs its own visibility toggle here).
  var isCockpit = name === 'cockpit';
  document.getElementById('sb').style.display = isCockpit ? '' : 'none';

  _currentView = name;
  location.hash = name;
  closeDrawer();
  if (name === 'about') loadAbout();
  if (name === 'config') { renderDirtyBanner(); switchCfgTab(_currentCfgTab); }
}

var _currentCfgTab = 'boards';

// Activate a tab within the config view (files | boards | buses).
// Triggers a loadDebug() for boards/buses tabs to ensure fresh data.
function switchCfgTab(name) {
  _currentCfgTab = name;
  document.querySelectorAll('.cfg-tab').forEach(function (t) {
    t.classList.toggle('active', t.dataset.tab === name);
  });
  document.querySelectorAll('.cfg-tabpanel').forEach(function (p) {
    p.classList.toggle('active', p.id === 'cfg-tab-' + name);
  });
  if (name === 'boards') { loadDebug(); }
  if (name === 'buses') { loadDebug(); }
  if (name === 'files') { loadConfigs(); cfgStatus('', ''); }
  if (name === 'diag') { startDccPoll(); } else { stopDccPoll(); }
}


/* ── Cockpit helpers ────────────────────────────────────────────────── */

// CSS class for a generic light/signal/audio card.
// d.state < 0 means firmware is busy (transitioning); d.desired is the target state (0=off, >0=on).
function cls(d) {
  if ((_deviceTypes[d.type] || {}).category === 'static') return 'static';
  if (d.state < 0) return 'busy';
  if (d.desired > 0) return 'on';
  return 'off';
}

// CSS class for a traffic-light card.
// desired values: 0=off, 1=stop (red), 2=go (green), 3=flash.
function clsTraffic(d) {
  if (d.state < 0) return 'busy';
  if (d.desired === 2) return 'on';
  if (d.desired === 1) return 'stop';
  if (d.desired === 3) return 'flash';
  return 'off';
}

// Button label text for a generic device card (maps CSS class to i18n key).
function lbl(c) {
  if (c === 'static') return t('ck.static');
  if (c === 'busy') return t('ck.busy');
  if (c === 'on') return t('ck.on');
  return t('ck.off');
}

// Button tooltip text for a generic device card (mirrors lbl()).
function btnTip(c) {
  if (c === 'on') return t('ck.on_tip');
  if (c === 'off' || c === 'stop' || c === 'flash') return t('ck.off_tip');
  return '';
}

// List view (#135) state-dot CSS class for one device — same on/off/busy/
// stop/flash vocabulary as the card view's .dot (mirrors the per-category
// class dispatch in card(): traffic devices use clsTraffic(), everything
// else resolves to the same busy/on/off formula as cls()).
function ckDotClass(d) {
  if ((_deviceTypes[d.type] || {}).category === 'traffic') return clsTraffic(d);
  return cls(d);
}

// List view (#135) Action cell content for one device, mirroring the same
// category dispatch as card(): traffic (fixed 4-aspect) and signal/servo/
// i2cMotor/i2cServo (dynamic multi-state) devices have no single on/off
// toggle — their current state is shown by which button in the cluster is
// filled (.active), so no separate state text is needed alongside it.
// Returns the action cell's HTML so renderList() doesn't re-derive this
// dispatch itself.
function ckAction(d) {
  var busy = d.state < 0;
  if (SERVO_TYPES.indexOf(d.type) >= 0) {
    function sbtnServo(s) {
      var act = (s.v === 0 && d.desired === 0) ? 'active' : '';
      var onclick = s.v === 'REV' ? 'revServo(\'' + d.id + '\')' : 'setServo(\'' + d.id + '\',' + s.v + ')';
      var mk = SERVO_MEANING[s.l];
      var lblTxt = mk ? t(mk) : s.l;
      return '<button class="tbtn ' + s.c + ' ' + act + '" title="' + lblTxt + '" onclick="' + onclick + '" ' + (busy ? 'disabled' : '') + '>' + lblTxt + '</button>';
    }
    return '<div class="tbtns ck-tbtns">' + SERVO_STATES.map(sbtnServo).join('') + '</div>';
  }
  if (I2C_SERVO_TYPES.indexOf(d.type) >= 0) {
    var positions = d.positions || [];
    function pbtn(label, st, css) {
      var act = (d.desired === st && !busy) ? 'active' : '';
      return '<button class="tbtn ' + css + ' ' + act + '" onclick="setSig(\'' + d.id + '\',' + st + ')" ' + (busy ? 'disabled' : '') + '>' + label + '</button>';
    }
    var btnsP = pbtn(t('servo.stop'), 0, 't-stop');
    positions.forEach(function (p, i) {
      btnsP += pbtn(p.label || (t('de.card_pos') + ' ' + (i + 1)), i + 1, 't-go');
    });
    return '<div class="tbtns ck-tbtns">' + btnsP + '</div>';
  }
  if (I2C_MOTOR_TYPES.indexOf(d.type) >= 0) {
    var mstates = d.states || (d.speed !== undefined ? [{ speed: d.speed }] : []);
    function mbtn(label, st, css) {
      var act = (d.desired === st && !busy) ? 'active' : '';
      return '<button class="tbtn ' + css + ' ' + act + '" onclick="setSig(\'' + d.id + '\',' + st + ')" ' + (busy ? 'disabled' : '') + '>' + label + '</button>';
    }
    var btnsM = mbtn(t('servo.stop'), 0, 't-stop');
    mstates.forEach(function (s, i) {
      btnsM += mbtn(s.label || (t('de.card_state') + ' ' + (i + 1)), i + 1, 't-go');
    });
    return '<div class="tbtns ck-tbtns">' + btnsM + '</div>';
  }
  if ((_deviceTypes[d.type] || {}).category === 'signal') {
    var states = (_deviceTypes[d.type] || {}).states || [];
    function sbtnSig(s) {
      var act = (d.desired === s.v) ? 'active' : '';
      var mk = ASPECT_MEANING[s.l];
      var lblTxt = !mk ? s.l : ASPECT_MEANING_ONLY[s.l] ? t(mk) : ASPECT_CODE_ONLY[s.l] ? s.l : (s.l + ' · ' + t(mk));
      return '<button class="tbtn ' + s.c + ' ' + act + '" title="' + (mk ? t(mk) : s.l) + '" onclick="setSig(\'' + d.id + '\',' + s.v + ')">' + lblTxt + '</button>';
    }
    return '<div class="tbtns ck-tbtns">' + states.map(sbtnSig).join('') + '</div>';
  }
  if ((_deviceTypes[d.type] || {}).category === 'traffic') {
    function tbtnList(label, cls2, st) {
      var act = (d.desired === st) ? 'active' : '';
      return '<button class="tbtn t-' + cls2 + ' ' + act + '" onclick="setTraffic(\'' + d.id + '\',' + st + ')">' + label + '</button>';
    }
    return '<div class="tbtns ck-tbtns">' + tbtnList(t('aspect.off'), 'off', 0) + tbtnList(t('aspect.go'), 'go', 2) + tbtnList(t('aspect.caution'), 'flash', 3) + tbtnList(t('aspect.stop'), 'stop', 1) + '</div>';
  }
  // Generic on/off device (incl. static, read-only).
  var cc = cls(d);
  if (cc === 'static') return '';
  return '<button class="btn ' + cc + '" title="' + btnTip(cc) + '" onclick="tog(\'' + d.id + '\',' + d.desired + ')">' + lbl(cc) + '</button>';
}

// Renders the metadata tag strip below the device icon:
//   – DCC address badge (if configured)
//   – For UART servo: "servo ID"
//   – For GPIO devices: "pin X,Y"
// Pins equal to 255 are sentinel values meaning "not connected" and are filtered out.
// The board/card number is wiring-internal detail with no operational value to
// the person running the cockpit, so it's omitted here too (#135, mirrors the
// same cut already made in the List view's _ckLocStr()).
function meta(d) {
  var h = '<div class="meta">';
  if (d.addr > 0) h += '<span class="mtag">DCC ' + d.addr + '</span>';
  if (d.servoId !== undefined) {
    h += '<span class="mtag">' + t('de.lbl_wiring_servo') + ' ' + d.servoId + '</span>';
  } else {
    var pins = d.pins ? d.pins.filter(function (p) { return p !== 255; }) : [];
    var pinStr = pins.length > 0 ? pins.join(', ') : null;
    // mtag-pin (as opposed to plain .mtag): lets compact density hide just
    // the pin reference while keeping DCC visible (#135) - in the narrow
    // compact card width, showing both truncated the DCC value itself,
    // which matters more than wiring detail in that dense view.
    if (pinStr) h += '<span class="mtag mtag-pin">pin ' + pinStr + '</span>';
  }
  return h + '</div>';
}

// Render a traffic-light device card with fixed OFF / GO / FLASH / STOP buttons.
function cardTraffic(d) {
  var c = clsTraffic(d);
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  // Build one traffic-light button. Buttons stay clickable during a transition
  // (busy): the firmware is interruptible and converges to the last request.
  function tbtn(label, cls, st) {
    var act = (d.desired === st) ? 'active' : '';
    return '<button class="tbtn t-' + cls + ' ' + act + '" onclick="setTraffic(\'' + d.id + '\',' + st + ')">' + label + '</button>';
  }
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + dtLabel(d.type) + '</span>'
    + meta(d)
    + '<div class="tbtns">' + tbtn(t('aspect.off'), 'off', 0) + tbtn(t('aspect.go'), 'go', 2) + tbtn(t('aspect.caution'), 'flash', 3) + tbtn(t('aspect.stop'), 'stop', 1) + '</div>'
    + '</div>';
}

// Railway aspect code -> i18n meaning key, for domain-meaningful cockpit buttons
// (e.g. HP1 -> "Voie libre"). Codes without a meaning fall back to the raw label.
var ASPECT_MEANING = {
  OFF: 'aspect.off',
  HP0: 'aspect.stop', HP00: 'aspect.stop',
  HP1: 'aspect.clear', HP2: 'aspect.slow', 'HP0+Sh1': 'aspect.shunting'
};

// Compound aspect codes (already long on their own) show as code-only on the
// button — the ' · meaning' suffix stays in the tooltip instead of wrapping
// the button label onto two lines.
var ASPECT_CODE_ONLY = { 'HP0+Sh1': true };

// "OFF" isn't a real railway aspect code (unlike HP0/HP1/HP2) — it's just the
// absence of one — so per the confirmed labelling rule (code+meaning only
// where a real domain code exists) it shows meaning-only, consistent with
// how generic on/off devices already read "Turn off" rather than a raw code.
var ASPECT_MEANING_ONLY = { OFF: true };

// SERVO_STATES label (STOP/SLOW/MID/FAST/REV, from device_types.json) -> i18n tooltip key.
var SERVO_MEANING = {
  STOP: 'servo.stop', SLOW: 'servo.slow', MID: 'servo.mid', FAST: 'servo.fast', REV: 'servo.rev'
};

// Render a railway signal card with dynamic state buttons from device_types.json.
function cardSignal(d) {
  var states = (_deviceTypes[d.type] || {}).states;
  var c = d.state < 0 ? 'busy' : (d.desired > 0 ? 'on' : 'off');
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  // Build one signal state button; s carries {v: value, c: css-class, l: label}.
  // Label shows "code · meaning" (e.g. HP1 · Voie libre); meaning is internationalised.
  // Buttons stay clickable during the POV transition (busy): newState() updates
  // the target unconditionally, so the firmware converges to the last request.
  function sbtn(s) {
    var act = (d.desired === s.v) ? 'active' : '';
    var mk = ASPECT_MEANING[s.l];
    var lbl = !mk ? s.l : ASPECT_MEANING_ONLY[s.l] ? t(mk) : ASPECT_CODE_ONLY[s.l] ? s.l : (s.l + ' · ' + t(mk));
    return '<button class="tbtn ' + s.c + ' ' + act + '" title="' + (mk ? t(mk) : s.l) + '" onclick="setSig(\'' + d.id + '\',' + s.v + ')">' + lbl + '</button>';
  }
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + dtLabel(d.type) + '</span>'
    + meta(d)
    + '<div class="tbtns">' + (states || []).map(sbtn).join('') + '</div>'
    + '</div>';
}

// Render a UART servo card with STOP / SLOW / MID / FAST / REV speed preset buttons.
// Only the STOP button shows as "active" (speed is not reported in /api/devices responses).
function cardServo(d) {
  var busy = d.state < 0;
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  var c = busy ? 'busy' : (d.desired > 0 ? 'on' : 'off');
  // Build one servo speed button; s carries {v: speed-value, c: css-class, l: label}.
  // Buttons stay clickable during a transition (interruptible, last request wins).
  function sbtn(s) {
    var act = (s.v === 0 && d.desired === 0) ? 'active' : '';
    var onclick = s.v === 'REV'
      ? 'revServo(\'' + d.id + '\')'
      : 'setServo(\'' + d.id + '\',' + s.v + ')';
    var mk = SERVO_MEANING[s.l];
    var lblTxt = mk ? t(mk) : s.l;
    return '<button class="tbtn ' + s.c + ' ' + act + '" title="' + lblTxt + '" onclick="' + onclick + '">' + lblTxt + '</button>';
  }
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + dtLabel(d.type) + '</span>'
    + meta(d)
    + '<div class="tbtns">' + SERVO_STATES.map(sbtn).join('') + '</div>'
    + '</div>';
}

// Render a continuous I²C motor card (PCA9685Motor).
// State 0 = STOP (off / neutral), states 1..N = motor states from d.states[].
function cardI2cMotor(d) {
  var states = d.states || (d.speed !== undefined ? [{ speed: d.speed }] : []);
  var busy = d.state < 0;
  var dis = busy ? 'disabled' : '';
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  var c = busy ? 'busy' : (d.desired > 0 ? 'on' : 'off');
  function mbtn(label, st, css) {
    var act = (d.desired === st && !busy) ? 'active' : '';
    return '<button class="tbtn ' + css + ' ' + act + '" onclick="setSig(\'' + d.id + '\',' + st + ')" ' + dis + '>' + label + '</button>';
  }
  var btns = mbtn(t('servo.stop'), 0, 't-stop');
  states.forEach(function (s, i) {
    btns += mbtn(s.label || (t('de.card_state') + ' ' + (i + 1)), i + 1, 't-go');
  });
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + dtLabel(d.type) + '</span>'
    + meta(d)
    + '<div class="tbtns">' + btns + '</div>'
    + '</div>';
}

// Render a positional I²C servo card (PCA9685Servo).
// State 0 = STOP (emergency stop), states 1..N = positions from d.positions[].
function cardI2cServo(d) {
  var positions = d.positions || [];
  var busy = d.state < 0;
  var dis = busy ? 'disabled' : '';
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  var c = busy ? 'busy' : (d.desired > 0 ? 'on' : 'off');
  function pbtn(label, st, css) {
    var act = (d.desired === st && !busy) ? 'active' : '';
    return '<button class="tbtn ' + css + ' ' + act + '" onclick="setSig(\'' + d.id + '\',' + st + ')" ' + dis + '>' + label + '</button>';
  }
  var btns = pbtn(t('servo.stop'), 0, 't-stop');
  positions.forEach(function (p, i) {
    btns += pbtn(p.label || (t('de.card_pos') + ' ' + (i + 1)), i + 1, 't-go');
  });
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + dtLabel(d.type) + '</span>'
    + meta(d)
    + '<div class="tbtns">' + btns + '</div>'
    + '</div>';
}

// Dispatcher: routes to the specialised card renderer based on device category.
function card(d) {
  if ((_deviceTypes[d.type] || {}).category === 'traffic') return cardTraffic(d);
  if (SERVO_TYPES.indexOf(d.type) >= 0) return cardServo(d);
  if (I2C_SERVO_TYPES.indexOf(d.type) >= 0) return cardI2cServo(d);
  if (I2C_MOTOR_TYPES.indexOf(d.type) >= 0) return cardI2cMotor(d);
  if ((_deviceTypes[d.type] || {}).category === 'signal') return cardSignal(d);
  var c = cls(d);
  // 'static' devices are read-only; 'busy' (mid-transition) stays clickable — the
  // firmware is interruptible and converges to the last request.
  var dis = (c === 'static') ? 'disabled' : '';
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + dtLabel(d.type) + '</span>'
    + meta(d)
    + '<button class="btn ' + c + '" title="' + btnTip(c) + '" onclick="tog(\'' + d.id + '\',' + d.desired + ')" ' + dis + '>' + lbl(c) + '</button>'
    + '</div>';
}

var _ckAllDevs = [];
// Persisted (#135) alongside view/density below, so the active type filter
// also survives a reload instead of silently resetting to "all types".
var _ckTypeFilter = localStorage.getItem('mrj-ck-type-filter') || null;

// Cockpit view mode ('type' = grouped by device type, current default;
// 'packed' = one flat auto-fill grid sorted by id; 'addr' = grouped by DCC
// address, #10 — address as a grouping key even without a real DCC bus;
// 'list' = sortable table with the config comment column, #85) and card
// density ('comfortable' default; 'compact' = smaller cards, tighter gaps).
// Both persisted in localStorage (#1) so the choice survives a reload.
var _ckView = localStorage.getItem('mrj-ck-view') || 'type';
var _ckDensity = localStorage.getItem('mrj-ck-density') || 'comfortable';

// List view (#85) sort state: column key + ascending flag. Not persisted —
// resets to id/asc on reload, matching the other views' lack of sort memory.
var _ckSortCol = 'id';
var _ckSortAsc = true;

function setCkView(mode) {
  _ckView = (mode === 'packed' || mode === 'addr' || mode === 'list') ? mode : 'type';
  localStorage.setItem('mrj-ck-view', _ckView);
  document.querySelectorAll('.ck-view-btn').forEach(function (b) {
    b.classList.toggle('active', b.getAttribute('data-view') === _ckView);
  });
  // Density (comfortable/compact) only affects the card-grid views — List is a
  // table with no notion of card density, so showing both as simultaneously
  // "active" would be meaningless (#135). Hide the whole group instead of
  // just disabling it.
  document.getElementById('ck-density-group').style.display = (_ckView === 'list') ? 'none' : '';
  if (_ckView === 'list' && !_dbgCfg) loadDebug().then(ckApplyFilters);
  else ckApplyFilters();
}

function setCkDensity(mode) {
  _ckDensity = (mode === 'compact') ? 'compact' : 'comfortable';
  localStorage.setItem('mrj-ck-density', _ckDensity);
  document.getElementById('grid').classList.toggle('ck-compact', _ckDensity === 'compact');
  document.querySelectorAll('.ck-density-btn').forEach(function (b) {
    b.classList.toggle('active', b.getAttribute('data-density') === _ckDensity);
  });
}

// Build type-filter pill buttons from the unique device types in devs.
// Label: strip the "MrJDB" namespace prefix, then insert spaces before capitals
//   ("MrJDBEntrySignal" → "Entry Signal").
function ckBuildTypeFilters(devs) {
  var seen = [];
  devs.forEach(function (d) { if (seen.indexOf(d.type) < 0) seen.push(d.type); });
  var html = seen.map(function (type) {
    var ico = ICONS[type] || ICONS['_'];
    var tip = tooltip(type);
    var active = _ckTypeFilter === type ? ' ck-tf-active' : '';
    var label = type.replace(/^MrJDB/, '').replace(/([A-Z])/g, ' $1').trim();
    return '<button class="ck-tf' + active + '" title="' + tip + '" onclick="ckToggleType(\'' + type + '\')">'
      + ico + '<span class="ck-tf-lbl">' + label + '</span></button>';
  }).join('');
  document.getElementById('ck-type-filters').innerHTML = html;
}

// Toggle the active type filter (clicking the same pill again clears the filter).
function ckToggleType(type) {
  _ckTypeFilter = (_ckTypeFilter === type) ? null : type;
  if (_ckTypeFilter) localStorage.setItem('mrj-ck-type-filter', _ckTypeFilter);
  else localStorage.removeItem('mrj-ck-type-filter');
  ckApplyFilters();
}

// Apply search text + type filter, rebuild filter pills, and re-render the grid
// in whichever view mode is currently selected (#1).
function ckApplyFilters() {
  var search = (document.getElementById('ck-search').value || '').trim().toLowerCase();
  var devs = _ckAllDevs.filter(function (d) {
    if (_ckTypeFilter && d.type !== _ckTypeFilter) return false;
    if (search && d.id.toLowerCase().indexOf(search) < 0) return false;
    return true;
  });
  ckBuildTypeFilters(_ckAllDevs);
  if (_ckView === 'packed') renderPacked(devs);
  else if (_ckView === 'addr') renderByAddr(devs);
  else if (_ckView === 'list') renderList(devs);
  else renderGrid(devs);
}

// Packed view (#1): a single flat auto-fill grid, no per-type grouping, sorted
// by id — packs the full row width instead of leaving small groups' rows half
// empty. No group-level ON/OFF buttons (there's no group).
function renderPacked(devs) {
  var html;
  if (devs.length === 0 && _ckAllDevs.length > 0) {
    html = '<div class="prm-info">' + t('ck.no_match') + '</div>';
  } else {
    var sorted = devs.slice().sort(function (a, b) { return a.id < b.id ? -1 : a.id > b.id ? 1 : 0; });
    html = '<div class="gcards ck-packed">' + sorted.map(card).join('') + '</div>';
  }
  document.getElementById('grid').innerHTML = html;
}

// Render the device grid, grouping cards by type with group-level ON/OFF buttons.
// Static devices skip the group buttons (they are read-only).
function renderGrid(devs) {
  var order = [];
  var groups = {};
  devs.forEach(function (d) {
    if (!groups[d.type]) { groups[d.type] = []; order.push(d.type); }
    groups[d.type].push(d);
  });
  var html = '';
  if (devs.length === 0 && _ckAllDevs.length > 0) {
    html = '<div class="prm-info">' + t('ck.no_match') + '</div>';
  }
  order.forEach(function (type) {
    var list = groups[type];
    var isStatic = (_deviceTypes[type] || {}).category === 'static';
    html += '<div class="group">';
    html += '<div class="ghdr"><span class="gname">' + type + ' <span class="gcnt">(' + list.length + ')</span></span>';
    if (!isStatic) {
      html += '<div class="gbtns">'
        + '<button class="gbtn on" title="' + t('ck.grp_on_tip') + '" onclick="groupDevices(\'' + type + '\',1)">' + t('ck.grp_on') + '</button>'
        + '<button class="gbtn off" title="' + t('ck.grp_off_tip') + '" onclick="groupDevices(\'' + type + '\',0)">' + t('ck.grp_off') + '</button>'
        + '</div>';
    }
    html += '</div>';
    html += '<div class="gcards">' + list.map(card).join('') + '</div>';
    html += '</div>';
  });
  document.getElementById('grid').innerHTML = html;
}

// Render the device grid, grouping cards by DCC address (#10) — lets a group
// of devices sharing one address (e.g. several lamps on the same decoder
// address) be switched together even without a real DCC bus. Devices with no
// address (addr <= 0) can't be grouped this way; they're listed last under a
// single unaddressed bucket with no group buttons. skip_delay is never sent
// from this UI, so each member's configured startup delay (#8) still applies.
function renderByAddr(devs) {
  var order = [];
  var groups = {};
  var unaddressed = [];
  devs.forEach(function (d) {
    if (!(d.addr > 0)) { unaddressed.push(d); return; }
    if (!groups[d.addr]) { groups[d.addr] = []; order.push(d.addr); }
    groups[d.addr].push(d);
  });
  order.sort(function (a, b) { return a - b; });
  var html = '';
  if (devs.length === 0 && _ckAllDevs.length > 0) {
    html = '<div class="prm-info">' + t('ck.no_match') + '</div>';
  }
  order.forEach(function (addr) {
    var list = groups[addr];
    html += '<div class="group">';
    html += '<div class="ghdr"><span class="gname">DCC ' + addr + ' <span class="gcnt">(' + list.length + ')</span></span>';
    html += '<div class="gbtns">'
      + '<button class="gbtn on" title="' + t('ck.grp_on_tip') + '" onclick="groupByAddr(' + addr + ',1)">' + t('ck.grp_on') + '</button>'
      + '<button class="gbtn off" title="' + t('ck.grp_off_tip') + '" onclick="groupByAddr(' + addr + ',0)">' + t('ck.grp_off') + '</button>'
      + '</div>';
    html += '</div>';
    html += '<div class="gcards">' + list.map(card).join('') + '</div>';
    html += '</div>';
  });
  if (unaddressed.length > 0) {
    html += '<div class="group">';
    html += '<div class="ghdr"><span class="gname">' + t('ck.addr_none') + ' <span class="gcnt">(' + unaddressed.length + ')</span></span></div>';
    html += '<div class="gcards">' + unaddressed.map(card).join('') + '</div>';
    html += '</div>';
  }
  document.getElementById('grid').innerHTML = html;
}

// Plain-text "board N · pin X,Y" / "GPIO X,Y" / servo bus string for a device,
// for the list view's Carte/pin column — same source data as meta(d)'s tags,
// but flattened to one cell instead of separate <span> tags.
function _ckLocStr(d) {
  if (d.servoId !== undefined) {
    return t('de.lbl_wiring_servo') + ' ' + d.servoId;
  }
  var pins = d.pins ? d.pins.filter(function (p) { return p !== 255; }) : [];
  var pinStr = pins.length > 0 ? pins.join(', ') : '';
  if (pinStr) return 'pin ' + pinStr;
  return '';
}

// Set the list view's sort column (click a header to sort by it; click again
// to reverse direction, like a typical sortable table) then re-render (#85).
function ckSortBy(col) {
  if (_ckSortCol === col) _ckSortAsc = !_ckSortAsc;
  else { _ckSortCol = col; _ckSortAsc = true; }
  ckApplyFilters();
}

// List view (#85): a sortable table of all matching devices, one row per
// device, with the config-only "comment" note (#83) as its own column — the
// only cockpit view that surfaces it. Comments live in config, not in the
// runtime /api/devices payload, so they're merged here from _dbgCfg (loaded
// on demand by setCkView() the first time this view is opened), the same
// merge pattern app-boards.js already uses for the Boards debug view.
function renderList(devs) {
  if (devs.length === 0 && _ckAllDevs.length > 0) {
    document.getElementById('grid').innerHTML = '<div class="prm-info">' + t('ck.no_match') + '</div>';
    return;
  }
  var comments = {};
  ((_dbgCfg && _dbgCfg.devices) || []).forEach(function (cd) {
    if (cd.comment) comments[cd.id] = cd.comment;
  });

  var cols = [
    { key: 'id', label: t('ck.col_id') },
    { key: 'type', label: t('ck.col_type') },
    { key: 'addr', label: t('ck.col_addr') },
    { key: 'loc', label: t('ck.col_loc') },
    { key: 'comment', label: t('ck.col_comment') }
  ];
  var rows = devs.map(function (d) {
    return {
      d: d,
      id: d.id,
      type: dtLabel(d.type),
      action: ckAction(d),
      addr: d.addr > 0 ? d.addr : 0,
      loc: _ckLocStr(d),
      comment: comments[d.id] || ''
    };
  });
  rows.sort(function (a, b) {
    var av = a[_ckSortCol], bv = b[_ckSortCol];
    var cmp = (typeof av === 'number' && typeof bv === 'number') ? (av - bv)
      : String(av).toLowerCase().localeCompare(String(bv).toLowerCase());
    return _ckSortAsc ? cmp : -cmp;
  });

  var html = '<table class="ck-table"><thead><tr><th></th>';
  cols.forEach(function (col) {
    var sortInd = (_ckSortCol !== col.key) ? '' : (_ckSortAsc ? ' ▲' : ' ▼');
    html += '<th onclick="ckSortBy(\'' + col.key + '\')">' + col.label + sortInd + '</th>';
  });
  html += '<th>' + t('ck.col_action') + '</th></tr></thead><tbody>';
  rows.forEach(function (r) {
    var d = r.d;
    var addrStr = d.addr > 0 ? 'DCC ' + d.addr : '';
    var commentStr = r.comment ? r.comment.replace(/</g, '&lt;') : '';
    var ico = ICONS[d.type] || ICONS['_'];
    var dotCls = ckDotClass(d);
    html += '<tr>'
      + '<td class="ck-table-ico" title="' + r.type + '">' + ico + '</td>'
      + '<td><span class="ck-table-id"><span class="dot ' + dotCls + '"></span>' + d.id + '</span></td>'
      + '<td>' + r.type + '</td>'
      + '<td>' + addrStr + '</td>'
      + '<td>' + r.loc + '</td>'
      + '<td>' + commentStr + '</td>'
      + '<td>' + r.action + '</td>'
      + '</tr>';
  });
  html += '</tbody></table>';
  document.getElementById('grid').innerHTML = html;
}

// Top-level cockpit renderer. Manages toolbar visibility and empty-state CTA.
// An empty device list after the first poll triggers the first-run welcome flow.
var _ckToolbarSynced = false;
function render(devs) {
  _ckAllDevs = devs;
  var toolbar = document.getElementById('ck-toolbar');
  var grid = document.getElementById('grid');
  if (devs.length === 0) {
    toolbar.style.display = 'none';
    var hasBoards = _dbgCfg && _dbgCfg.boards && _dbgCfg.boards.length > 0;
    var ctaBtn = (hasBoards ? '<button class="ck-empty-btn" onclick="goToBoards()">' + t('ck.empty_btn') + '</button>' : '')
      + '<button class="ck-empty-btn" onclick="openWizard()">' + t('ck.setup_btn') + '</button>';
    grid.innerHTML = '<div class="ck-empty">'
      + '<div class="ck-empty-ico">🎛</div>'
      + '<div class="ck-empty-title">' + t('ck.empty_title') + '</div>'
      + '<div class="ck-empty-body">' + t('ck.empty_body') + '</div>'
      + ctaBtn
      + '</div>';
  } else {
    toolbar.style.display = '';
    // The view/density/type-filter buttons' .active state and the density
    // group's visibility default to whatever's hardcoded in webui.html, which
    // doesn't necessarily match the mode restored from localStorage above —
    // sync them once so a reload doesn't just restore the right *content*
    // while the toolbar still visually points at the previous session's mode
    // (#135). Only once: after this, clicks handle their own sync.
    if (!_ckToolbarSynced) {
      _ckToolbarSynced = true;
      document.querySelectorAll('.ck-view-btn').forEach(function (b) {
        b.classList.toggle('active', b.getAttribute('data-view') === _ckView);
      });
      document.querySelectorAll('.ck-density-btn').forEach(function (b) {
        b.classList.toggle('active', b.getAttribute('data-density') === _ckDensity);
      });
      grid.classList.toggle('ck-compact', _ckDensity === 'compact');
      document.getElementById('ck-density-group').style.display = (_ckView === 'list') ? 'none' : '';
      if (_ckView === 'list' && !_dbgCfg) { loadDebug().then(ckApplyFilters); return; }
    }
    ckBuildTypeFilters(devs);
    ckApplyFilters();
  }
  var now = new Date().toLocaleTimeString('fr-FR');
  document.getElementById('sb').innerHTML = '<span>' + devs.length + '</span> appareils &mdash; ' + now;
}

/* ── API calls ──────────────────────────────────────────────────────── */

// POST JSON body to url; returns the raw fetch Promise (caller handles .then/.catch).
function post(url, body) {
  return fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
}

// DELETE request with a JSON body; resolves to the parsed response JSON or throws on error.
function deleteJson(url, body) {
  return fetch(url, { method: 'DELETE', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); });
}

// Zero-pad a number to at least 2 digits.
function pad2(n) { return n < 10 ? '0' + n : '' + n; }

// Show the persistent error banner (connection lost / firmware unreachable).
// Also marks that we were in error state so the wizard re-triggers on reconnect.
var _pollErrState = false;
function showErr() {
  document.getElementById('err').style.display = 'block';
  _pollErrState = true;
}

var _firstPollDone = false;

// Periodic cockpit poll: fetch /api/devices, render cards, trigger welcome wizard when
// the device list is empty (first poll, or first successful poll after a reconnect).
function poll() {
  fetch('/api/devices')
    .then(function (r) { if (!r.ok) throw r; return r.json(); })
    .then(function (d) {
      if (_pollErrState) { _pollErrState = false; _firstPollDone = false; } // reset on reconnect
      document.getElementById('err').style.display = 'none';
      render(d);
      if (_currentView === 'config') { _dbgDevs = d; renderDebugBoards(); }
      if (!_firstPollDone) {
        _firstPollDone = true;
        if (d.length === 0) showWelcome();
      }
    })
    .catch(showErr);
}
