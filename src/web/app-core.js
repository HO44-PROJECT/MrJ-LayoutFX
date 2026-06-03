/**
 * @file app-core.js
 * @brief Cockpit view: globals, navigation, device card renderers, filter, grid, poll.
 *
 * Core WebUI module bundled with other app-*.js modules by build_webui.py.
 * Manages device cards, real-time polling, and main navigation.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
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
// Hides/shows the header action buttons and status bar (cockpit-only).
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

  // Show global actions + status bar only on cockpit
  var isCockpit = name === 'cockpit';
  document.getElementById('hdr-actions').style.display = isCockpit ? '' : 'none';
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

// Renders the metadata tag strip below the device icon:
//   – DCC address badge (if configured)
//   – For UART servo: "Carte N · servo ID"
//   – For GPIO devices: "Carte N · pin X,Y" or "GPIO X,Y"
// Pins equal to 255 are sentinel values meaning "not connected" and are filtered out.
function meta(d) {
  var h = '<div class="meta">';
  if (d.addr > 0) h += '<span class="mtag">DCC ' + d.addr + '</span>';
  if (d.servoId !== undefined) {
    // UART servo: no GPIO pins, just a bus ID
    var boardLabel = d.board > 0 ? 'Carte ' + d.board + ' · ' : '';
    h += '<span class="mtag">' + boardLabel + t('de.lbl_wiring_servo') + ' ' + d.servoId + '</span>';
  } else {
    var pins = d.pins ? d.pins.filter(function (p) { return p !== 255; }) : [];
    var pinStr = pins.length > 0 ? pins.join(', ') : null;
    if (d.board > 0) h += '<span class="mtag">Carte ' + d.board + (pinStr ? ' · pin ' + pinStr : '') + '</span>';
    else if (pinStr) h += '<span class="mtag">GPIO ' + pinStr + '</span>';
  }
  return h + '</div>';
}

// Render a traffic-light device card with fixed OFF / GO / FLASH / STOP buttons.
function cardTraffic(d) {
  var c = clsTraffic(d);
  var busy = d.state < 0;
  var dis = busy ? 'disabled' : '';
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  // Build one traffic-light button; marks it active when d.desired matches state st.
  function tbtn(label, cls, st) {
    var act = (d.desired === st && !busy) ? 'active' : '';
    return '<button class="tbtn t-' + cls + ' ' + act + '" onclick="setTraffic(\'' + d.id + '\',' + st + ')" ' + dis + '>' + label + '</button>';
  }
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + d.type + '</span>'
    + meta(d)
    + '<div class="tbtns">' + tbtn('OFF', 'off', 0) + tbtn('GO', 'go', 2) + tbtn('FLASH', 'flash', 3) + tbtn('STOP', 'stop', 1) + '</div>'
    + '</div>';
}

// Render a railway signal card with dynamic state buttons from device_types.json.
function cardSignal(d) {
  var states = (_deviceTypes[d.type] || {}).states;
  var c = d.state < 0 ? 'busy' : (d.desired > 0 ? 'on' : 'off');
  var busy = d.state < 0;
  var dis = busy ? 'disabled' : '';
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  // Build one signal state button; s carries {v: value, c: css-class, l: label}.
  function sbtn(s) {
    var act = (d.desired === s.v && !busy) ? 'active' : '';
    return '<button class="tbtn ' + s.c + ' ' + act + '" onclick="setSig(\'' + d.id + '\',' + s.v + ')" ' + dis + '>' + s.l + '</button>';
  }
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + d.type + '</span>'
    + meta(d)
    + '<div class="tbtns">' + states.map(sbtn).join('') + '</div>'
    + '</div>';
}

// Render a UART servo card with STOP / SLOW / MID / FAST / REV speed preset buttons.
// Only the STOP button shows as "active" (speed is not reported in /api/devices responses).
function cardServo(d) {
  var busy = d.state < 0;
  var dis = busy ? 'disabled' : '';
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  var c = busy ? 'busy' : (d.desired > 0 ? 'on' : 'off');
  // Build one servo speed button; s carries {v: speed-value, c: css-class, l: label}.
  // active = STOP button when off, no active highlight for speed buttons (speed not in /api/devices response)
  function sbtn(s) {
    var act = (s.v === 0 && d.desired === 0 && !busy) ? 'active' : '';
    var onclick = s.v === 'REV'
      ? 'revServo(\'' + d.id + '\')'
      : 'setServo(\'' + d.id + '\',' + s.v + ')';
    return '<button class="tbtn ' + s.c + ' ' + act + '" onclick="' + onclick + '" ' + dis + '>' + s.l + '</button>';
  }
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + d.type + '</span>'
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
  var btns = mbtn('STOP', 0, 't-stop');
  states.forEach(function (s, i) {
    btns += mbtn(s.label || (t('de.card_state') + ' ' + (i + 1)), i + 1, 't-go');
  });
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + d.type + '</span>'
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
  var btns = pbtn('STOP', 0, 't-stop');
  positions.forEach(function (p, i) {
    btns += pbtn(p.label || (t('de.card_pos') + ' ' + (i + 1)), i + 1, 't-go');
  });
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + d.type + '</span>'
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
  var dis = (c === 'busy' || c === 'static') ? 'disabled' : '';
  var ico = ICONS[d.type] || ICONS['_'];
  var tip = tooltip(d.type);
  return '<div class="card ' + c + '">'
    + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
    + '<span class="dot ' + c + '"></span></div>'
    + '<div class="icon" title="' + tip + '">' + ico + '</div>'
    + '<span class="badge">' + d.type + '</span>'
    + meta(d)
    + '<button class="btn ' + c + '" onclick="tog(\'' + d.id + '\',' + d.desired + ')" ' + dis + '>' + lbl(c) + '</button>'
    + '</div>';
}

var _ckAllDevs = [];
var _ckTypeFilter = null;

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
  ckApplyFilters();
}

// Apply search text + type filter, rebuild filter pills, and re-render the grid.
function ckApplyFilters() {
  var search = (document.getElementById('ck-search').value || '').trim().toLowerCase();
  var devs = _ckAllDevs.filter(function (d) {
    if (_ckTypeFilter && d.type !== _ckTypeFilter) return false;
    if (search && d.id.toLowerCase().indexOf(search) < 0) return false;
    return true;
  });
  ckBuildTypeFilters(_ckAllDevs);
  renderGrid(devs);
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
        + '<button class="gbtn on" onclick="groupDevices(\'' + type + '\',1)">' + t('ck.grp_on') + '</button>'
        + '<button class="gbtn off" onclick="groupDevices(\'' + type + '\',0)">' + t('ck.grp_off') + '</button>'
        + '</div>';
    }
    html += '</div>';
    html += '<div class="gcards">' + list.map(card).join('') + '</div>';
    html += '</div>';
  });
  document.getElementById('grid').innerHTML = html;
}

// Top-level cockpit renderer. Manages toolbar visibility and empty-state CTA.
// An empty device list after the first poll triggers the first-run welcome flow.
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
