
/**
 * @file  app.js
 * @brief SPA JavaScript for the MrJ-ArduinoRailwayFX WebUI.
 *        Bundled into webui.html by build_webui.py; comments stripped by rjsmin at build time.
 *
 * Architecture overview:
 *   Cockpit view  — polls /api/devices every POLL ms, renders device cards grouped by type.
 *   Config view   — boards tab (DIP diagram + device/board editors), buses tab, files tab.
 *   About view    — system status fetched once on open.
 *
 *   All runtime data cached in _dbg* globals; UI re-rendered from those caches.
 *
 *   Three JSON catalogues fetched lazily on first config-view open:
 *     /api/board-types  → _boardTypes
 *     /api/device-types → _deviceTypes  (+ derived category lists)
 *     /api/bus-types    → _busTypes
 *     /api/i2c-known    → _i2cKnown
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

var _currentCfgTab = 'files';

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
    var boardLabel = d.board > 0 ? 'Carte ' + d.board + ' \u00b7 ' : '';
    h += '<span class="mtag">' + boardLabel + t('de.lbl_wiring_servo') + '\u00a0' + d.servoId + '</span>';
  } else {
    var pins = d.pins ? d.pins.filter(function (p) { return p !== 255; }) : [];
    var pinStr = pins.length > 0 ? pins.join(',\u202f') : null;
    if (d.board > 0) h += '<span class="mtag">Carte ' + d.board + (pinStr ? ' \u00b7 pin\u00a0' + pinStr : '') + '</span>';
    else if (pinStr) h += '<span class="mtag">GPIO\u00a0' + pinStr + '</span>';
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

// Render a positional I²C servo card (PCA9685Servo).
// State 0 = STOP (emergency stop), states 1..N = positions from d.positions[].
// Position labels come from config; falls back to "Pos N" if absent.
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
    // If boards are already configured, guide to config/boards instead of reopening the wizard.
    var hasBoards = _dbgCfg && _dbgCfg.boards && _dbgCfg.boards.length > 0;
    var ctaBtn = hasBoards
      ? '<button class="ck-empty-btn" onclick="goToBoards()">' + t('ck.empty_btn') + '</button>'
      : '<button class="ck-empty-btn" onclick="openWizard()">' + t('ck.setup_btn') + '</button>';
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

// ── Setup wizard (first boot) ────────────────────────────────────────────────
// Replaces the old static welcome modal.  Triggered when the first /api/devices
// poll returns an empty list AND no boards are configured yet.
// The wizard collects board, buses and expansion cards in 3–4 steps, builds the
// config in memory, saves it once, and calls POST /api/reload — no restart.

var _wiz = null;          // null = wizard closed
var _wizExpIdx = 0;       // monotonic counter for expansion-board _idx

// Entry point — called by showWelcome() after config + status + board-types are loaded.
function _wizOpen(status) {
  var env = ((status && status.env) || '').toLowerCase().replace(/[_\s-]/g, '');
  var defaultType = _ENV_TO_BOARD[env] || guessDefaultBoardType();
  var boardId = defaultType.toLowerCase().replace(/[^a-z0-9]/g, '');

  _wizExpIdx = 0;
  _wiz = {
    step: 1,
    mainBoardType: defaultType,
    mainBoardId: boardId,
    buses: {
      i2c: { enabled: false, key: 'i2c0', sda: 21, scl: 22 },
      spi: { enabled: false, key: 'spi', mosi: 23, sclk: 18, latch: 5 },
      uart: { enabled: false, key: 'uart1', tx: 17, rx: 16, baud: 115200 }
    },
    expansionBoards: [],
    configName: ''
  };

  document.getElementById('wizard-overlay').style.display = 'block';
  document.getElementById('wizard-modal').style.display = 'flex';
  _wizRender();
}

function _wizClose() {
  document.getElementById('wizard-overlay').style.display = 'none';
  document.getElementById('wizard-modal').style.display = 'none';
  _wiz = null;
}

// How many steps total (3 when no buses enabled — step 3 expansion is skipped).
function _wizTotalSteps() {
  if (!_wiz) return 4;
  return (_wiz.buses.i2c.enabled || _wiz.buses.spi.enabled || _wiz.buses.uart.enabled) ? 4 : 3;
}

// Map actual step number to display step number (step 4 becomes "3" when total=3).
function _wizDisplayStep(step) {
  if (_wizTotalSteps() === 3 && step === 4) return 3;
  return step;
}

// ── Step renderers ───────────────────────────────────────────────────────────

function _wizRenderStep1() {
  var mcuTypes = Object.keys(_boardTypes).filter(function (k) {
    var bt = _boardTypes[k]; return bt && !bt.busType;
  });
  if (mcuTypes.length === 0) mcuTypes = ['ESP32DevkitC'];
  var opts = mcuTypes.map(function (k) {
    var def = _boardTypes[k] || {};
    return '<option value="' + k + '"' + (k === _wiz.mainBoardType ? ' selected' : '') + '>'
      + (def.label || k) + '</option>';
  }).join('');
  return '<div class="de-field">'
    + '<label>Type de carte</label>'
    + '<select id="wiz-board-type" onchange="wizUpdateBoardId()">' + opts + '</select>'
    + '</div>'
    + '<div class="de-field">'
    + '<label>Identifiant</label>'
    + '<input type="text" id="wiz-board-id" value="' + _wiz.mainBoardId + '" autocomplete="off" placeholder="ex: mainboard">'
    + '</div>';
}

function _wizBusCard(busKey, label, fields) {
  var bus = _wiz.buses[busKey];
  var fieldsHtml = fields.map(function (f) {
    return '<div class="de-field" style="margin-top:.35rem">'
      + '<label style="font-size:.8rem">' + f.label + '</label>'
      + '<input type="number"' + (bus.enabled ? '' : ' disabled')
      + ' id="wiz-' + busKey + '-' + f.fk + '"'
      + ' value="' + (bus[f.fk] !== undefined ? bus[f.fk] : f.ph) + '"'
      + ' min="' + f.min + '" max="' + f.max + '" placeholder="' + f.ph + '">'
      + '</div>';
  }).join('');
  return '<div class="wiz-bus-card' + (bus.enabled ? ' wiz-bus-card--on' : '') + '" id="wiz-buscard-' + busKey + '">'
    + '<div class="wiz-bus-header">'
    + '<label class="wiz-bus-toggle">'
    + '<input type="checkbox"' + (bus.enabled ? ' checked' : '')
    + ' onchange="wizToggleBus(\'' + busKey + '\')">'
    + '<span class="wiz-bus-label">' + label + '</span>'
    + '</label>'
    + '</div>'
    + '<div class="wiz-bus-fields" id="wiz-busfields-' + busKey + '"'
    + (bus.enabled ? '' : ' style="display:none"') + '>'
    + fieldsHtml
    + '</div>'
    + '</div>';
}

function _wizRenderStep2() {
  var feats = (_dbgStatus && _dbgStatus.features) || {};
  var html = _wizBusCard('i2c', 'I²C', [
    { fk: 'sda', label: 'SDA (GPIO)', min: 0, max: 39, ph: 21 },
    { fk: 'scl', label: 'SCL (GPIO)', min: 0, max: 39, ph: 22 }
  ]);
  if (feats.spi !== false) {
    html += _wizBusCard('spi', 'SPI', [
      { fk: 'mosi', label: 'MOSI (GPIO)', min: 0, max: 39, ph: 23 },
      { fk: 'sclk', label: 'SCLK (GPIO)', min: 0, max: 39, ph: 18 },
      { fk: 'latch', label: 'LATCH (GPIO)', min: 0, max: 39, ph: 5 }
    ]);
  }
  html += _wizBusCard('uart', 'Série (UART)', [
    { fk: 'tx', label: 'TX (GPIO)', min: 0, max: 39, ph: 17 },
    { fk: 'rx', label: 'RX (GPIO)', min: 0, max: 39, ph: 16 },
    { fk: 'baud', label: 'Baud', min: 300, max: 921600, ph: 115200 }
  ]);
  return html;
}

function _wizExpBoardsForBus(busLocalKey) {
  var bts = { i2c: 'i2c', spi: 'spi_master_only', uart: 'uart' }[busLocalKey];
  return Object.keys(_boardTypes).filter(function (k) {
    return _boardTypes[k] && _boardTypes[k].busType === bts;
  });
}

function _wizRenderExpBoardRow(b) {
  var compat = _wizExpBoardsForBus(b.busLocalKey);
  var typeOpts = compat.map(function (k) {
    var def = _boardTypes[k] || {};
    return '<option value="' + k + '"' + (k === b.type ? ' selected' : '') + '>'
      + (def.label || k) + '</option>';
  }).join('');
  var i2cField = b.busLocalKey === 'i2c'
    ? '<div class="de-field" style="margin-top:.3rem">'
    + '<label style="font-size:.8rem">Adresse I²C</label>'
    + '<input type="number" value="' + b.i2cAddress + '" min="0" max="127" '
    + 'onchange="wizUpdateExpBoard(' + b._idx + ',\'i2cAddress\',+this.value)">'
    + '</div>'
    : '';
  return '<div class="wiz-expboard-row">'
    + '<button class="wiz-del-board" onclick="wizDeleteExpBoard(' + b._idx + ')">✕</button>'
    + '<div class="de-field">'
    + '<label style="font-size:.8rem">Type</label>'
    + '<select onchange="wizUpdateExpBoard(' + b._idx + ',\'type\',this.value)">' + typeOpts + '</select>'
    + '</div>'
    + '<div class="de-field">'
    + '<label style="font-size:.8rem">ID</label>'
    + '<input type="text" value="' + b.id + '" placeholder="ex: pca1" '
    + 'onchange="wizUpdateExpBoard(' + b._idx + ',\'id\',this.value)">'
    + '</div>'
    + i2cField
    + '</div>';
}

function _wizRenderExpSection(busLocalKey, busLabel) {
  var compat = _wizExpBoardsForBus(busLocalKey);
  if (compat.length === 0) return '';
  var rows = _wiz.expansionBoards
    .filter(function (b) { return b.busLocalKey === busLocalKey; })
    .map(_wizRenderExpBoardRow).join('');
  return '<div class="wiz-expboard-section">'
    + '<p class="wiz-section-title">' + busLabel + '</p>'
    + '<div id="wiz-expboards-' + busLocalKey + '">' + rows + '</div>'
    + '<button class="wiz-add-board" onclick="wizAddExpBoard(\'' + busLocalKey + '\')">'
    + '+ Ajouter une carte</button>'
    + '</div>';
}

function _wizRenderStep3() {
  var html = '';
  if (_wiz.buses.i2c.enabled) html += _wizRenderExpSection('i2c', 'Bus I²C (' + _wiz.buses.i2c.key + ')');
  if (_wiz.buses.spi.enabled) html += _wizRenderExpSection('spi', 'Bus SPI (' + _wiz.buses.spi.key + ')');
  if (_wiz.buses.uart.enabled) html += _wizRenderExpSection('uart', 'Bus Série (' + _wiz.buses.uart.key + ')');
  if (!html) {
    html = '<p style="color:var(--t3);font-size:.85rem">Aucune carte d\'extension disponible pour les bus configurés.</p>';
  }
  return html;
}

function _wizRenderStep4() {
  var lines = [];
  var bt = _boardTypes[_wiz.mainBoardType];
  lines.push('<strong>Carte :</strong> ' + ((bt && bt.label) || _wiz.mainBoardType)
    + ' <span style="color:var(--t3)">(id: ' + _wiz.mainBoardId + ')</span>');
  var b = _wiz.buses;
  if (b.i2c.enabled) lines.push('<strong>I²C :</strong> SDA=' + b.i2c.sda + ', SCL=' + b.i2c.scl);
  if (b.spi.enabled) lines.push('<strong>SPI :</strong> MOSI=' + b.spi.mosi + ', SCLK=' + b.spi.sclk + ', LATCH=' + b.spi.latch);
  if (b.uart.enabled) lines.push('<strong>Série :</strong> TX=' + b.uart.tx + ', RX=' + b.uart.rx + ', Baud=' + b.uart.baud);
  _wiz.expansionBoards.forEach(function (eb) {
    var def = _boardTypes[eb.type] || {};
    lines.push('<strong>Extension :</strong> ' + (def.label || eb.type)
      + ' <span style="color:var(--t3)">(id: ' + eb.id + ', bus: ' + eb.busKey + ')</span>');
  });
  return '<div class="de-field">'
    + '<label>Nom de la configuration</label>'
    + '<input type="text" id="wiz-cfg-name" value="' + (_wiz.configName || 'ma-maquette') + '" '
    + 'placeholder="ma-maquette" autocomplete="off">'
    + '</div>'
    + '<div class="wiz-summary">'
    + '<p class="wiz-summary-title">Résumé</p>'
    + lines.map(function (l) { return '<p class="wiz-summary-line">' + l + '</p>'; }).join('')
    + '</div>'
    + '<div id="wiz-err" class="wiz-err" style="display:none"></div>';
}

// ── Render ────────────────────────────────────────────────────────────────────

function _wizRender() {
  if (!_wiz) return;
  var modal = document.getElementById('wizard-modal');
  if (!modal) return;

  var total = _wizTotalSteps();
  var step = _wiz.step;
  var display = _wizDisplayStep(step);
  var pct = Math.round(display / total * 100);

  var titles = ['Carte principale', 'Bus de communication', 'Cartes d\'extension', 'Nom et résumé'];
  var title = step <= 4 ? titles[step - 1] : '';
  if (total === 3 && step === 4) title = titles[3]; // summary when step 3 skipped

  var body = '';
  if (step === 1) body = _wizRenderStep1();
  else if (step === 2) body = _wizRenderStep2();
  else if (step === 3) body = _wizRenderStep3();
  else if (step === 4) body = _wizRenderStep4();

  var isFirst = (step === 1);
  var isLast = (step === 4);

  modal.innerHTML =
    '<div class="wizard-header">'
    + '<div class="wizard-progress-track"><div class="wizard-progress-fill" style="width:' + pct + '%"></div></div>'
    + '<div class="wizard-step-info">Étape ' + display + ' / ' + total + '</div>'
    + '<h2 class="wizard-title">' + title + '</h2>'
    + '</div>'
    + '<div class="wizard-body">' + body + '</div>'
    + '<div class="wizard-footer">'
    + (isFirst ? '' : '<button class="wizard-btn wizard-btn-back" onclick="wizBack()">← Précédent</button>')
    + '<span style="flex:1"></span>'
    + (isLast
      ? '<button class="wizard-btn wizard-btn-finish" id="wiz-finish-btn" onclick="wizFinish()">✓ Appliquer</button>'
      : '<button class="wizard-btn wizard-btn-next" onclick="wizNext()">Suivant →</button>')
    + '</div>';
}

// ── State save helpers ────────────────────────────────────────────────────────

function _wizSaveStep1() {
  var typeEl = document.getElementById('wiz-board-type');
  var idEl = document.getElementById('wiz-board-id');
  if (typeEl) _wiz.mainBoardType = typeEl.value;
  if (idEl) _wiz.mainBoardId = idEl.value.trim()
    || (_wiz.mainBoardType || '').toLowerCase().replace(/[^a-z0-9]/g, '');
}

function _wizSaveStep2() {
  ['i2c', 'spi', 'uart'].forEach(function (bk) {
    var container = document.getElementById('wiz-busfields-' + bk);
    if (!container) return;
    container.querySelectorAll('input[type=number]').forEach(function (el) {
      var fk = el.id.replace('wiz-' + bk + '-', '');
      if (el.value !== '') _wiz.buses[bk][fk] = Number(el.value);
    });
  });
}

function _wizSaveCfgName() {
  var el = document.getElementById('wiz-cfg-name');
  if (el) _wiz.configName = el.value.trim();
}

function _wizSaveCurrent() {
  if (_wiz.step === 1) _wizSaveStep1();
  if (_wiz.step === 2) _wizSaveStep2();
  if (_wiz.step === 4) _wizSaveCfgName();
}

// ── Nav ───────────────────────────────────────────────────────────────────────

function wizBack() {
  if (!_wiz) return;
  _wizSaveCurrent();
  _wiz.step--;
  // Skip step 3 if no buses (going backward)
  if (_wiz.step === 3 && _wizTotalSteps() === 3) _wiz.step--;
  _wizRender();
}

function wizNext() {
  if (!_wiz) return;
  _wizSaveCurrent();
  _wiz.step++;
  // Skip step 3 if no buses enabled
  if (_wiz.step === 3 && _wizTotalSteps() === 3) _wiz.step++;
  _wizRender();
}

// ── Interactivity ─────────────────────────────────────────────────────────────

function wizUpdateBoardId() {
  var typeEl = document.getElementById('wiz-board-type');
  var idEl = document.getElementById('wiz-board-id');
  if (typeEl && idEl && _wiz) {
    _wiz.mainBoardType = typeEl.value;
    idEl.value = typeEl.value.toLowerCase().replace(/[^a-z0-9]/g, '');
    _wiz.mainBoardId = idEl.value;
  }
}

function wizToggleBus(busKey) {
  if (!_wiz) return;
  _wizSaveStep2();
  _wiz.buses[busKey].enabled = !_wiz.buses[busKey].enabled;
  var card = document.getElementById('wiz-buscard-' + busKey);
  var fields = document.getElementById('wiz-busfields-' + busKey);
  if (!card || !fields) return;
  if (_wiz.buses[busKey].enabled) {
    card.classList.add('wiz-bus-card--on');
    fields.style.display = '';
    fields.querySelectorAll('input').forEach(function (el) { el.disabled = false; });
  } else {
    card.classList.remove('wiz-bus-card--on');
    fields.style.display = 'none';
    fields.querySelectorAll('input').forEach(function (el) { el.disabled = true; });
  }
}

function wizAddExpBoard(busLocalKey) {
  if (!_wiz) return;
  var compat = _wizExpBoardsForBus(busLocalKey);
  if (compat.length === 0) return;
  var busKey = _wiz.buses[busLocalKey].key;
  var type = compat[0];
  var id = type.toLowerCase().replace(/[^a-z0-9]/g, '') + (_wizExpIdx + 1);
  _wiz.expansionBoards.push({
    _idx: _wizExpIdx++, busLocalKey: busLocalKey, busKey: busKey,
    type: type, id: id, i2cAddress: 64
  });
  var cont = document.getElementById('wiz-expboards-' + busLocalKey);
  if (cont) {
    cont.innerHTML = _wiz.expansionBoards
      .filter(function (b) { return b.busLocalKey === busLocalKey; })
      .map(_wizRenderExpBoardRow).join('');
  }
}

function wizUpdateExpBoard(idx, field, value) {
  if (!_wiz) return;
  var board = _wiz.expansionBoards.find(function (b) { return b._idx === idx; });
  if (!board) return;
  board[field] = value;
  if (field === 'type') {
    board.id = value.toLowerCase().replace(/[^a-z0-9]/g, '') + (idx + 1);
    var cont = document.getElementById('wiz-expboards-' + board.busLocalKey);
    if (cont) {
      cont.innerHTML = _wiz.expansionBoards
        .filter(function (b) { return b.busLocalKey === board.busLocalKey; })
        .map(_wizRenderExpBoardRow).join('');
    }
  }
}

function wizDeleteExpBoard(idx) {
  if (!_wiz) return;
  var board = _wiz.expansionBoards.find(function (b) { return b._idx === idx; });
  var bk = board ? board.busLocalKey : null;
  _wiz.expansionBoards = _wiz.expansionBoards.filter(function (b) { return b._idx !== idx; });
  if (bk) {
    var cont = document.getElementById('wiz-expboards-' + bk);
    if (cont) {
      cont.innerHTML = _wiz.expansionBoards
        .filter(function (b) { return b.busLocalKey === bk; })
        .map(_wizRenderExpBoardRow).join('');
    }
  }
}

// ── Config builder ────────────────────────────────────────────────────────────

function _wizBuildConfig() {
  var cfg = { buses: {}, boards: [], devices: [] };
  if (_wiz.configName) cfg.name = _wiz.configName;

  // Main MCU board (never has a bus)
  cfg.boards.push({ id: _wiz.mainBoardId, type: _wiz.mainBoardType });

  // Buses
  var b = _wiz.buses;
  if (b.i2c.enabled) cfg.buses[b.i2c.key] = { type: 'i2c', sda: b.i2c.sda, scl: b.i2c.scl };
  if (b.spi.enabled) cfg.buses[b.spi.key] = { type: 'spi_master_only', mosi: b.spi.mosi, sclk: b.spi.sclk, latch: b.spi.latch };
  if (b.uart.enabled) cfg.buses[b.uart.key] = { type: 'uart', tx: b.uart.tx, rx: b.uart.rx, baud: b.uart.baud };

  // Expansion boards
  _wiz.expansionBoards.forEach(function (eb) {
    var entry = { id: eb.id, type: eb.type, bus: eb.busKey };
    if (eb.busLocalKey === 'i2c') entry.i2c_address = eb.i2cAddress;
    cfg.boards.push(entry);
  });

  return cfg;
}

// ── Finish ────────────────────────────────────────────────────────────────────

function wizFinish() {
  if (!_wiz) return;
  _wizSaveCurrent();
  var cfg = _wizBuildConfig();

  var btn = document.getElementById('wiz-finish-btn');
  var errEl = document.getElementById('wiz-err');
  if (btn) { btn.disabled = true; btn.textContent = 'Application…'; }
  if (errEl) errEl.style.display = 'none';

  fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(cfg)
  })
    .then(function (r) { if (!r.ok) throw new Error('Sauvegarde impossible (HTTP ' + r.status + ')'); return r.json(); })
    .then(function () {
      return fetch('/api/reload', { method: 'POST' });
    })
    .then(function (r) {
      if (!r.ok) throw new Error('Rechargement impossible (HTTP ' + r.status + ')');
      _dbgCfg = cfg;
      _wizClose();
      setTimeout(function () { loadDebug(); poll(); }, 800);
    })
    .catch(function (e) {
      if (btn) { btn.disabled = false; btn.textContent = '✓ Appliquer'; }
      if (errEl) { errEl.textContent = e.message; errEl.style.display = 'block'; }
    });
}

// ── Trigger (called from poll) ────────────────────────────────────────────────

// Called when the first /api/devices poll returns an empty list.
function showWelcome() {
  var pStatus = _dbgStatus
    ? Promise.resolve(_dbgStatus)
    : fetch('/api/status').then(function (r) { return r.json(); }).catch(function () { return {}; });

  var pBoardTypes = Object.keys(_boardTypes).length > 0
    ? Promise.resolve(_boardTypes)
    : fetch('/api/board-types').then(function (r) { return r.json(); }).catch(function () { return {}; });

  Promise.all([
    fetch('/api/config').then(function (r) {
      if (r.status === 404) return { buses: {}, boards: [], devices: [] };
      if (!r.ok) throw new Error('cfg ' + r.status);
      return r.json();
    }),
    pStatus,
    pBoardTypes
  ])
    .then(function (res) {
      var cfg = res[0], status = res[1], bt = res[2];
      _dbgCfg = cfg;
      if (status && status.env) _dbgStatus = status;
      if (bt && Object.keys(bt).length > 0) _boardTypes = bt;
      loadSystemPins(cfg);

      if (cfg.boards && cfg.boards.length > 0) return; // already configured — skip
      _wizOpen(status);
    })
    .catch(function (e) { console.error('[wizard]', e); });
}

// Kept for backward compat (old HTML might reference it — not used anymore).
function closeWelcome() { _wizClose(); }

// Open the setup wizard manually, bypassing the "already configured" guard.
// Warns the user if devices already exist, because the wizard will overwrite the
// entire config (buses + boards) and strip all devices on apply.
function openWizard() {
  var pStatus = _dbgStatus
    ? Promise.resolve(_dbgStatus)
    : fetch('/api/status').then(function (r) { return r.json(); }).catch(function () { return {}; });

  var pBoardTypes = Object.keys(_boardTypes).length > 0
    ? Promise.resolve(_boardTypes)
    : fetch('/api/board-types').then(function (r) { return r.json(); }).catch(function () { return {}; });

  Promise.all([
    fetch('/api/config').then(function (r) {
      if (r.status === 404) return { buses: {}, boards: [], devices: [] };
      if (!r.ok) throw new Error('cfg ' + r.status);
      return r.json();
    }),
    pStatus,
    pBoardTypes
  ])
    .then(function (res) {
      var cfg = res[0], status = res[1], bt = res[2];
      _dbgCfg = cfg;
      if (status && status.env) _dbgStatus = status;
      if (bt && Object.keys(bt).length > 0) _boardTypes = bt;
      loadSystemPins(cfg);

      var devCount = (cfg.devices || []).filter(function (d) { return !d.type || !d.type.startsWith('$'); }).length;
      if (devCount > 0 && !confirm(t('cfg.wizard_warn').replace('{n}', devCount))) return;

      _wizOpen(status);
    })
    .catch(function (e) { console.error('[wizard]', e); });
}

// Factory-reset the config: wipe buses + devices, keep only a single default board.
// Board type is guessed from /api/status env; falls back to first available MCU type.
function resetConfig() {
  if (!confirm(t('cfg.reset_confirm'))) return;
  var env = (_dbgStatus && _dbgStatus.env || '').toLowerCase().replace(/[_\s-]/g, '');
  var defaultType = _ENV_TO_BOARD[env] || 'ESP32DevkitC';
  var btKeys = Object.keys(_boardTypes).filter(function (k) { return !(_boardTypes[k].busType); });
  if (btKeys.length > 0 && !_boardTypes[defaultType]) defaultType = btKeys[0];
  var boardId = defaultType.toLowerCase().replace(/[^a-z0-9]/g, '');
  var cfg = { buses: {}, boards: [{ id: boardId, type: defaultType }], devices: [] };
  fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(cfg)
  })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { clearDirty(); location.reload(); })
    .catch(function (e) { alert('Reset failed: ' + e.message); });
}

// Fetch the generated main.cpp from /api/export/code and trigger a browser download.
// Button is only shown when running on localhost (dev mode).
function exportCode() {
  fetch('/api/export/code')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
    .then(function (code) {
      var blob = new Blob([code], { type: 'text/plain' });
      var url = URL.createObjectURL(blob);
      var a = document.createElement('a');
      a.href = url;
      a.download = 'main.cpp';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    })
    .catch(function (e) { alert(t('cfg.export_err') + ': ' + e.message); });
}

// Toggle a generic light/audio device (inverts desired state).
function tog(id, desired) {
  post('/api/device', { id: id, state: desired > 0 ? 0 : 1 }).then(poll).catch(showErr);
}

// Set traffic-light state directly (0=off, 1=stop, 2=go, 3=flash).
function setTraffic(id, state) {
  post('/api/device', { id: id, state: state }).then(poll).catch(showErr);
}

// Set a railway signal to a specific state index.
function setSig(id, state) {
  post('/api/device', { id: id, state: state }).then(poll).catch(showErr);
}

// Set a UART servo speed (0=stop, or a speed value in ms from SERVO_STATES).
function setServo(id, speed) {
  post('/api/servo', { id: id, speed: speed }).then(poll).catch(showErr);
}

// Send a reverse direction command to a UART servo.
function revServo(id) {
  post('/api/servo', { id: id, action: 'reverse' }).then(poll).catch(showErr);
}

// Turn all devices on or off across all boards.
function allDevices(state) {
  post('/api/all', { state: state }).then(poll).catch(showErr);
}

// Turn all devices of a given type on or off.
function groupDevices(type, state) {
  post('/api/group', { type: type, state: state }).then(poll).catch(showErr);
}

/* ── Config file switcher ───────────────────────────────────────────── */

var CFG_PENDING_KEY = 'mrjfx_pending_cfg'; // persisted: file to activate on next restart
var _cfgRenaming = null;                   // filename currently being renamed inline
var _cfgActive = 'config.json';          // current active source filename (from API)

// Generate a compact timestamp string for snapshot filenames: "YYYYMMDD_HHMM".
function _cfgTs() {
  var now = new Date();
  return now.getFullYear() + pad2(now.getMonth() + 1) + pad2(now.getDate())
    + '_' + pad2(now.getHours()) + pad2(now.getMinutes());
}

// Fetch /api/configs and render the file list with action buttons.
// The active file gets Snapshot/Download buttons; pending gets an extra Cancel option;
// others get a Choose button to make them the next active config after restart.
// When _cfgRenaming is set, that row renders as an inline text input instead.
function loadConfigs() {
  fetch('/api/configs')
    .then(function (r) { return r.json(); })
    .then(function (data) {
      _cfgActive = data.active || 'config.json';
      var pending = localStorage.getItem(CFG_PENDING_KEY);
      var el = document.getElementById('cfg-filelist');
      el.innerHTML = data.files.map(function (entry) {
        // Support both old format (string) and new format ({file,name})
        var f = (typeof entry === 'string') ? entry : entry.file;
        var cname = (typeof entry === 'object' && entry.name) ? entry.name : '';
        var isActive = (f === data.active);
        var isPending = (!isActive && f === pending);
        var cls = 'cfg-fileitem' + (isActive ? ' active' : '') + (isPending ? ' pending' : '');
        var dot = '<span class="cfg-filedot"></span>';
        var sf = f.replace(/\\/g, '\\\\').replace(/'/g, "\\'");

        // Inline rename mode
        if (f === _cfgRenaming) {
          return '<div class="' + cls + '">' + dot
            + '<input id="cfg-rename-inp" class="cfg-rename-inp" type="text" maxlength="27"'
            + ' value="' + f.replace(/\.json$/, '') + '"'
            + ' onkeydown="if(event.key===\'Enter\')cfgRenameConfirm(\'' + sf + '\');'
            + 'if(event.key===\'Escape\')cfgRenameCancel()">'
            + '<button class="cfg-row-btn ok" onclick="cfgRenameConfirm(\'' + sf + '\')" title="OK">✓</button>'
            + '<button class="cfg-row-btn" onclick="cfgRenameCancel()" title="Annuler">✕</button>'
            + '</div>';
        }

        var ghostRename = '<span class="cfg-row-btn cfg-row-ghost" aria-hidden="true">' + t('cfg.rename.btn') + '</span>';
        var ghostDelete = '<span class="cfg-row-btn cfg-row-ghost" aria-hidden="true">✕ ' + t('cfg.destroy.btn') + '</span>';
        var btns = '<div class="cfg-row-btns">';
        if (isActive) {
          btns += '<span class="cfg-filebadge">' + t('cfg.badge.active') + '</span>';
          btns += '<button class="cfg-row-btn" onclick="cfgSnapshot(_cfgActive)">' + t('cfg.snapshot.btn') + '</button>';
          btns += '<button class="cfg-row-btn" onclick="cfgDownload(_cfgActive,_cfgActive)">⬇ ' + t('cfg.dl.btn') + '</button>';
          if (_dbgStatus && _dbgStatus.local) btns += '<button class="cfg-row-btn" onclick="cfgDownloadCpp(_cfgActive)">⬇ main.cpp</button>';
          if (_dbgStatus && _dbgStatus.local) btns += '<button class="cfg-row-btn" onclick="cfgDownloadPio(_cfgActive)">⬇ platformio.ini</button>';
          btns += ghostRename + ghostDelete;
        } else if (isPending) {
          btns += '<span class="cfg-filebadge pending">' + t('cfg.badge.pending') + '</span>';
          btns += '<button class="cfg-row-btn" onclick="cfgSnapshot(\'' + sf + '\')">' + t('cfg.snapshot.btn') + '</button>';
          btns += '<button class="cfg-row-btn" onclick="cfgDownload(\'' + sf + '\')">⬇ ' + t('cfg.dl.btn') + '</button>';
          if (_dbgStatus && _dbgStatus.local) btns += '<button class="cfg-row-btn" onclick="cfgDownloadCpp(\'' + sf + '\')">⬇ main.cpp</button>';
          if (_dbgStatus && _dbgStatus.local) btns += '<button class="cfg-row-btn" onclick="cfgDownloadPio(\'' + sf + '\')">⬇ platformio.ini</button>';
          btns += '<button class="cfg-row-btn" onclick="cfgRename(\'' + sf + '\')">' + t('cfg.rename.btn') + '</button>';
          btns += '<button class="cfg-row-btn danger" onclick="cfgDelete(\'' + sf + '\')">✕ ' + t('cfg.destroy.btn') + '</button>';
        } else {
          btns += '<button class="cfg-row-btn primary" onclick="cfgChoose(\'' + sf + '\')">' + t('cfg.choose_btn') + '</button>';
          btns += '<button class="cfg-row-btn" onclick="cfgSnapshot(\'' + sf + '\')">' + t('cfg.snapshot.btn') + '</button>';
          btns += '<button class="cfg-row-btn" onclick="cfgDownload(\'' + sf + '\')">⬇ ' + t('cfg.dl.btn') + '</button>';
          if (_dbgStatus && _dbgStatus.local) btns += '<button class="cfg-row-btn" onclick="cfgDownloadCpp(\'' + sf + '\')">⬇ main.cpp</button>';
          if (_dbgStatus && _dbgStatus.local) btns += '<button class="cfg-row-btn" onclick="cfgDownloadPio(\'' + sf + '\')">⬇ platformio.ini</button>';
          btns += '<button class="cfg-row-btn" onclick="cfgRename(\'' + sf + '\')">' + t('cfg.rename.btn') + '</button>';
          btns += '<button class="cfg-row-btn danger" onclick="cfgDelete(\'' + sf + '\')">✕ ' + t('cfg.destroy.btn') + '</button>';
        }
        btns += '</div>';

        return '<div class="' + cls + '">'
          + dot
          + '<span class="cfg-fname">' + f + '</span>'
          + '<span class="cfg-flayout">' + (cname ? cname : '') + '</span>'
          + btns
          + '</div>';
      }).join('');
    })
    .catch(function () {
      document.getElementById('cfg-filelist').textContent = '—';
    });
}

/* ── Per-row config actions ─────────────────────────────────────────── */

// Mark a config file as pending activation (applied on next ESP32 restart).
function cfgChoose(name) {
  localStorage.setItem(CFG_PENDING_KEY, name);
  loadConfigs();
  markDirty();
}

// Create a timestamped copy of a config file (base truncated to keep total ≤ 32 chars).
function cfgSnapshot(name) {
  var base = name.replace(/\.json$/, '').substring(0, 13); // 13 + '_' + 13chars_ts + '.json' ≤ 32
  var toName = base + '_' + _cfgTs() + '.json';
  post('/api/config/copy', { from: name, to: toName })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { cfgStatus('Snapshot → ' + toName, 'ok'); loadConfigs(); })
    .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
}

// Enter inline rename mode for a file row (replaces the filename span with a text input).
function cfgRename(name) {
  _cfgRenaming = name;
  loadConfigs();
  setTimeout(function () {
    var inp = document.getElementById('cfg-rename-inp');
    if (inp) { inp.focus(); inp.select(); }
  }, 0);
}

// Confirm the inline rename: auto-appends .json if absent, then POST /api/config/rename.
function cfgRenameConfirm(oldName) {
  var inp = document.getElementById('cfg-rename-inp');
  if (!inp) return;
  var newName = inp.value.trim();
  if (!newName) return;
  if (!newName.endsWith('.json')) newName += '.json';
  post('/api/config/rename', { from: oldName, to: newName })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { _cfgRenaming = null; cfgStatus('Renommé → ' + newName, 'ok'); loadConfigs(); })
    .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
}

// Cancel inline rename without saving.
function cfgRenameCancel() {
  _cfgRenaming = null;
  loadConfigs();
}

// Delete a config file from LittleFS (active file cannot be deleted by the server).
function cfgDelete(name) {
  if (!confirm('Supprimer ' + name + ' ?')) return;
  deleteJson('/api/configs', { file: name })
    .then(function () { cfgStatus('Fichier supprimé.', 'ok'); loadConfigs(); })
    .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
}

/* ── Config management ──────────────────────────────────────────────── */

// Update the upload filename label and enable/disable the upload button.
function onCfgFileSelect(input) {
  var name = input.files.length ? input.files[0].name : t('cfg.ul.nofile');
  document.getElementById('cfg-filename').textContent = name;
  document.getElementById('cfg-upload-btn').disabled = !input.files.length;
  cfgStatus('', '');
}

// Fetch a config file from LittleFS and trigger a browser download.
// suggestedName allows saving as the real layout filename instead of "config.json".
function cfgDownload(name, suggestedName) {
  fetch('/api/config/file?name=' + encodeURIComponent(name))
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.blob(); })
    .then(function (blob) {
      var url = URL.createObjectURL(blob);
      var a = document.createElement('a');
      a.href = url;
      a.download = suggestedName || name;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    })
    .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
}

// Download the currently active config file under its real layout filename.
function downloadConfig() { cfgDownload(_cfgActive, _cfgActive); }

// Trigger a browser download of a text blob.
function _downloadText(content, filename) {
  var blob = new Blob([content], { type: 'text/plain' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url; a.download = filename;
  document.body.appendChild(a); a.click();
  document.body.removeChild(a); URL.revokeObjectURL(url);
}

// Download main.cpp generated from a config file (local server only).
function cfgDownloadCpp(name) {
  fetch('/api/export/code?name=' + encodeURIComponent(name))
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
    .then(function (code) { _downloadText(code, 'main.cpp'); })
    .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
}

// Download platformio.ini generated from a config file (local server only).
function cfgDownloadPio(name) {
  fetch('/api/export/platformio-ini?name=' + encodeURIComponent(name))
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
    .then(function (ini) { _downloadText(ini, 'platformio.ini'); })
    .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
}

// Sanitize a filename to match LittleFS/server rules: alphanum + _ - . only, max 32 chars.
function _sanitizeCfgName(name) {
  // Strip path, keep only filename
  name = name.replace(/^.*[\\/]/, '');
  // Ensure .json extension
  if (!name.match(/\.json$/i)) name += '.json';
  // Replace any invalid char with underscore
  name = name.replace(/[^a-zA-Z0-9_\-.]/g, '_');
  // Truncate to 32 chars (LFS_NAME_MAX)
  if (name.length > 32) name = name.substring(0, 28) + '.json';
  return name;
}

// Validate and upload a JSON config file from the local filesystem to LittleFS.
// The file is JSON-parsed client-side first to reject malformed uploads early.
function uploadConfig() {
  var file = document.getElementById('cfg-file').files[0];
  if (!file) return;

  var safeName = _sanitizeCfgName(file.name);

  var reader = new FileReader();
  reader.onload = function (e) {
    try { JSON.parse(e.target.result); }
    catch (err) { cfgStatus('JSON invalide : ' + err.message, 'err'); return; }

    document.getElementById('cfg-upload-btn').disabled = true;
    cfgStatus('Envoi en cours… → ' + safeName, 'ok');

    fetch('/api/configs?name=' + encodeURIComponent(safeName), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Config-Name': safeName },
      body: e.target.result
    })
      .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
      .then(function () { cfgStatus(t('cfg.saved'), 'ok'); document.getElementById('cfg-upload-btn').disabled = false; loadConfigs(); })
      .catch(function (err) { cfgStatus(t('de.err_prefix') + err.message, 'err'); });
  };
  reader.readAsText(file);
}

// Apply pending config file switch (if any), then hot-reload the firmware config.
// If a pending file is set: activate it first, then trigger reload.
// No restart — everything reloads dynamically.
function applyEsp32() {
  cfgStatus(t('cfg.applying'), 'ok');
  var pending = localStorage.getItem(CFG_PENDING_KEY);
  var doReload = function () {
    fetch('/api/reload', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' })
      .then(function () {
        clearDirty();
        cfgStatus(t('cfg.applied'), 'ok');
        setTimeout(function () { loadDebug(); poll(); }, 800);
      })
      .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
  };
  if (pending) {
    post('/api/config/activate', { file: pending })
      .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
      .then(doReload)
      .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
  } else {
    doReload();
  }
}

// Show the reboot-in-progress modal overlay.
function showRebootModal() {
  document.getElementById('reboot-overlay').classList.add('active');
  document.getElementById('reboot-modal').style.display = 'block';
}

// Hide the reboot modal (used if the user cancels or the reconnect succeeds).
function hideRebootModal() {
  document.getElementById('reboot-overlay').classList.remove('active');
  document.getElementById('reboot-modal').style.display = 'none';
}

// Show the reboot modal and count down 8 seconds, then start polling for reconnection.
function startRebootCountdown() {
  showRebootModal();
  var n = 8;
  // Decrement the countdown every second; switches to reconnect polling when it reaches 0.
  function tick() {
    if (n > 0) {
      cfgStatus(t('cfg.rebooting', { n: n }), 'ok');
      document.getElementById('reboot-count').textContent = t('rst.count', { n: n });
      n--;
      setTimeout(tick, 1000);
    } else {
      document.getElementById('reboot-count').textContent = '';
      document.getElementById('reboot-msg').textContent = t('rst.reconnecting');
      cfgStatus(t('rst.reconnecting'), 'ok');
      tryReconnect();
    }
  }
  tick();
}

// Polls /api/status every second until the ESP32 is back online after a reboot,
// then clears the dirty flag and reloads the page.
function tryReconnect() {
  fetch('/api/status')
    .then(function (r) {
      if (r.ok) {
        clearDirty(); // remove localStorage flag before reload so banner doesn't reappear
        location.reload();
      } else {
        setTimeout(tryReconnect, 1000);
      }
    })
    .catch(function () { setTimeout(tryReconnect, 1000); });
}

// Show or hide the config-tab status message bar (cls: 'ok' | 'err' | 'warn').
// Pass an empty msg to hide it.
function cfgStatus(msg, cls) {
  var el = document.getElementById('cfg-status');
  el.style.display = msg ? '' : 'none';
  el.className = 'cfg-status ' + cls;
  el.textContent = msg;
}

/* ── Dirty state ────────────────────────────────────────────────────── */
// "Dirty" means config changes are pending that have not yet been applied to the ESP32
// (board/device/bus edits saved to config.json, or a pending config-file switch).
// Stored in localStorage so the warning banner survives page refresh.

var CFG_DIRTY_KEY = 'mrjfx_dirty';

// Mark config as dirty (pending file-switch — only used by cfgChoose).
function markDirty() {
  localStorage.setItem(CFG_DIRTY_KEY, '1');
  renderDirtyBanner();
}

// Clear the dirty flag and pending file selection.
function clearDirty() {
  localStorage.removeItem(CFG_DIRTY_KEY);
  localStorage.removeItem(CFG_PENDING_KEY);
  _cfgSelected = null;
  renderDirtyBanner();
}

// Show or hide the pending-file-switch banner based on localStorage state.
// Always hidden in local-server mode.
function renderDirtyBanner() {
  if (_dbgStatus && _dbgStatus.local) return;
  var dirty = !!localStorage.getItem(CFG_DIRTY_KEY);
  document.getElementById('dirty-banner').style.display = dirty ? '' : 'none';
}

// Trigger a hot-reload after any config mutation (device/board/bus save/delete).
// The reload is deferred to Core 1; after 1.5 s we refresh both runtime device states
// (poll) and the full board/config view (loadDebug) to pick up newly initialised devices.
function _reloadAfterSave() {
  fetch('/api/reload', { method: 'POST' }).catch(function () { });
  setTimeout(function () { loadDebug(); poll(); }, 1500);
}

/* ── Debug (mise au point) ──────────────────────────────────────────── */

// Strip JSON-schema meta-keys (starting with '$') from a catalogue object before storing.
function _stripMeta(obj) {
  return Object.fromEntries(Object.entries(obj).filter(function (e) { return !e[0].startsWith('$'); }));
}

var _boardTypes = {};  // from /api/board-types  (board_types.json)
var _deviceTypes = {};  // from /api/device-types (device_types.json)
var _busTypes = {};  // from /api/bus-types    (bus_types.json)
var _i2cKnown = {};  // from /api/i2c-known    (i2c_known.json)
var _dbgBoards = [];  // from /api/boards — runtime board list (spiRank, pinCount)
var _dbgDevs = [];  // from /api/devices — runtime device state
var _dbgCfg = null; // from /api/config — persisted config (source of truth for UI)
var _dbgStatus = null; // from /api/status — firmware build info + features + sys_pins
var _busDev = {};  // merged bus-device cache { id → mergedDev }, for openDevEditorById
var _dbgTest = {};  // client-side GPIO test state { 'g17': 0|1 } — not from firmware
var _dbgTestSpi = {};  // client-side SPI test state { 'c1_p9': 0|1 }
var _dbgSysPins = {};  // GPIO → label for pins reserved by buses: { 23:'MOSI', 18:'SCLK', … }
var _dbgFirmwarePins = {}; // GPIO → label from compile-time features (/api/status sys_pins)

// Populate _deviceTypes and the derived category globals from a freshly fetched dt object.
// SERVO_STATES uses SerialServo as the reference because all servo types share the same
// speed presets and REV action.
function _applyDeviceTypes(dt) {
  _deviceTypes = _stripMeta(dt);
  SERVO_TYPES = Object.keys(dt).filter(function (k) { return dt[k].category === 'servo'; });
  I2C_SERVO_TYPES = Object.keys(dt).filter(function (k) { return dt[k].category === 'i2c_servo'; });
  I2C_MOTOR_TYPES = Object.keys(dt).filter(function (k) { return dt[k].category === 'i2c_motor'; });
  STATIC_TYPES = Object.keys(dt).filter(function (k) { return dt[k].category === 'static'; });
  TRAFFIC_TYPES = Object.keys(dt).filter(function (k) { return dt[k].category === 'traffic'; });
  SERVO_STATES = (dt['SerialServo'] || {}).states || [];
}

// Build _dbgSysPins (GPIO → label) from config.json buses.
// Using cfg rather than /api/status sys_pins ensures that edits made in the UI
// are reflected immediately — sys_pins only updates after a firmware restart.
function loadSystemPins(cfg) {
  _dbgSysPins = {};
  if (!cfg) return;
  var buses = cfg.buses || {};
  Object.keys(buses).forEach(function (key) {
    var bus = buses[key];
    if (bus.type === 'dcc' && bus.pin >= 0) _dbgSysPins[bus.pin] = 'DCC';
    if (bus.type === 'spi_master_only') {
      if (bus.mosi >= 0) _dbgSysPins[bus.mosi] = 'MOSI';
      if (bus.sclk >= 0) _dbgSysPins[bus.sclk] = 'SCLK';
      if (bus.latch >= 0) _dbgSysPins[bus.latch] = 'LATCH';
    }
    if (bus.type === 'spi_full_duplex') {
      if (bus.mosi >= 0) _dbgSysPins[bus.mosi] = 'MOSI';
      if (bus.miso >= 0) _dbgSysPins[bus.miso] = 'MISO';
      if (bus.sclk >= 0) _dbgSysPins[bus.sclk] = 'SCLK';
      if (bus.cs >= 0) _dbgSysPins[bus.cs] = 'CS';
    }
    if (bus.type === 'i2c') {
      if (bus.sda >= 0) _dbgSysPins[bus.sda] = 'SDA';
      if (bus.scl >= 0) _dbgSysPins[bus.scl] = 'SCL';
    }
    if (bus.type === 'uart') {
      var shortName = key.replace('uart', 'U').toUpperCase();
      if (bus.tx >= 0) _dbgSysPins[bus.tx] = shortName + '\u00b7TX';
      if (bus.rx >= 0) _dbgSysPins[bus.rx] = shortName + '\u00b7RX';
    }
  });
}

// Fetch all data needed by the config view in parallel, then render.
// Device/bus/i2c catalogues are lazy: fetched only on first call, skipped afterwards.
// Firmware sys_pins are merged into _dbgSysPins with lower priority than config-declared buses.
function loadDebug() {
  var pDevs = fetch('/api/devices')
    .then(function (r) { if (!r.ok) throw r; return r.json(); })
    .then(function (devs) { _dbgDevs = devs; })
    .catch(function () { });
  var pTypes = fetch('/api/board-types')
    .then(function (r) { if (!r.ok) throw r; return r.json(); })
    .then(function (bt) { _boardTypes = _stripMeta(bt); })
    .catch(function () { });
  var pBoards = fetch('/api/boards')
    .then(function (r) { if (!r.ok) throw r; return r.json(); })
    .then(function (boards) { _dbgBoards = boards; })
    .catch(function () { });
  var pCfg = fetch('/api/config')
    .then(function (r) {
      if (r.status === 404) return { buses: {}, boards: [], devices: [] };
      if (!r.ok) throw r;
      return r.json();
    })
    .then(function (cfg) { _dbgCfg = cfg; loadSystemPins(cfg); })
    .catch(function () { });
  var pStatus = fetch('/api/status')
    .then(function (r) { if (!r.ok) throw r; return r.json(); })
    .then(function (st) {
      _dbgStatus = st;
      _dbgFirmwarePins = {};
      var sp = st.sys_pins || {};
      Object.keys(sp).forEach(function (k) { _dbgFirmwarePins[k] = sp[k]; });
    })
    .catch(function () { });
  // Meta definitions — fetched once, skipped if already loaded
  var pDevTypes = Object.keys(_deviceTypes).length > 0 ? Promise.resolve()
    : fetch('/api/device-types')
      .then(function (r) { if (!r.ok) throw r; return r.json(); })
      .then(function (dt) { _applyDeviceTypes(dt); })
      .catch(function () { });
  var pBusTypes = Object.keys(_busTypes).length > 0 ? Promise.resolve()
    : fetch('/api/bus-types')
      .then(function (r) { if (!r.ok) throw r; return r.json(); })
      .then(function (bt) { _busTypes = _stripMeta(bt); })
      .catch(function () { });
  var pI2cKnown = Object.keys(_i2cKnown).length > 0 ? Promise.resolve()
    : fetch('/api/i2c-known')
      .then(function (r) { if (!r.ok) throw r; return r.json(); })
      .then(function (ik) { _i2cKnown = _stripMeta(ik); })
      .catch(function () { });
  Promise.all([pDevs, pTypes, pBoards, pCfg, pStatus, pDevTypes, pBusTypes, pI2cKnown]).then(function () {
    // Merge firmware-reserved pins — config-declared buses take precedence.
    Object.keys(_dbgFirmwarePins).forEach(function (gpio) {
      if (!_dbgSysPins[gpio]) _dbgSysPins[gpio] = _dbgFirmwarePins[gpio];
    });
    applyLayoutName((_dbgCfg && _dbgCfg.name) || '');
    renderDebugBoards();
    if (_currentCfgTab === 'buses') renderBusesTab();
    if (_currentCfgTab === 'files') loadConfigs();
    var exportBtn = document.getElementById('cfg-export-btn');
    if (exportBtn) exportBtn.style.display = (_dbgStatus && _dbgStatus.ip === 'localhost') ? '' : 'none';
  });
}

// Manual refresh button handler for the config view.
function refreshDebug() { loadDebug(); }

// ── I2C scanner ──────────────────────────────────────────────────────────

// Trigger an I2C bus scan on the ESP32 and display found addresses with chip names from _i2cKnown.
function scanI2c() {
  var btn = document.getElementById('i2c-scan-btn');
  var res = document.getElementById('i2c-result');
  btn.disabled = true;
  btn.textContent = t('dbg.scan_i2c_scanning');
  res.style.display = 'none';
  fetch('/api/scan/i2c')
    .then(function (r) { return r.json(); })
    .then(function (d) {
      var sda = d.sda !== undefined ? d.sda : '?';
      var scl = d.scl !== undefined ? d.scl : '?';
      var html = '<span class="i2c-pins">SDA\u00a0GPIO' + sda + ' / SCL\u00a0GPIO' + scl + '</span> ';
      if (!d.found || d.found.length === 0) {
        html += '<span class="i2c-none">' + t('dbg.scan_i2c_none') + '</span>';
      } else {
        html += '<span class="i2c-label">' + t('dbg.scan_i2c_found') + ':</span> ';
        html += d.found.map(function (a) {
          var hex = '0x' + ('0' + a.toString(16).toUpperCase()).slice(-2);
          var name = (d.names && d.names[a]) ? ' <span class="i2c-name">' + d.names[a] + '</span>' : '';
          return '<span class="i2c-addr">' + hex + name + '</span>';
        }).join(' ');
      }
      res.innerHTML = html;
      res.style.display = 'flex';
    })
    .catch(function (e) {
      res.innerHTML = '<span class="i2c-none">Erreur: ' + e.message + '</span>';
      res.style.display = 'flex';
    })
    .finally(function () {
      btn.disabled = false;
      btn.textContent = t('dbg.scan_i2c');
    });
}

// Find a device by board API index (0-based) and wiring number.
// Firmware stores board as 1-based index, so boardApiIdx+1 is used for matching.
function dbgFindDev(boardApiIdx, wiring) {
  var boardFwIdx = boardApiIdx + 1;
  for (var i = 0; i < _dbgDevs.length; i++) {
    var d = _dbgDevs[i];
    if (d.board !== boardFwIdx) continue;
    var pins = d.pins && d.pins.length > 0 ? d.pins : [];
    if (pins.indexOf(wiring) >= 0) return d;
  }
  // Fallback: device in config but firmware not restarted yet
  var boardId = _dbgCfg && _dbgCfg.boards && _dbgCfg.boards[boardApiIdx]
    ? _dbgCfg.boards[boardApiIdx].id : null;
  if (!boardId) return null;
  var cfgDevs = (_dbgCfg && _dbgCfg.devices) || [];
  for (var j = 0; j < cfgDevs.length; j++) {
    var cd = cfgDevs[j];
    if (cd.board !== boardId) continue;
    var w = cd.wiring;
    var match = Array.isArray(w) ? w.indexOf(wiring) >= 0 : w === wiring;
    if (match) return { id: cd.id, type: cd.type, desired: -1, pins: [wiring], _cfgOnly: true };
  }
  return null;
}

// ── Rendering ────────────────────────────────────────────────────────

// Sync the layout name to both the header subtitle and the settings input field.
// Does not update the input if it currently has focus (prevents overwriting user typing).
function applyLayoutName(name) {
  var span = document.getElementById('hdr-layout-name');
  if (span) span.textContent = name ? '\u00a0\u2014\u00a0' + name : '';
  var inp = document.getElementById('cfg-layout-name');
  if (inp && inp !== document.activeElement) inp.value = name || '';
}

// Called on every keystroke in the layout name field; updates the header and auto-saves.
function onLayoutNameInput() {
  var name = (document.getElementById('cfg-layout-name').value || '').trim();
  applyLayoutName(name);
  if (!_dbgCfg) return;
  _dbgCfg.name = name || undefined;
  saveCfg(_dbgCfg);
}

// Persist the full config object to /api/config; clears dirty on success.
function saveCfg(cfg) {
  fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(cfg) })
    .then(function (r) { if (r.ok) { clearDirty(); _reloadAfterSave(); } })
    .catch(function () { });
}

// Renders the boards tab DIP diagram list.
// _dbgCfg.boards is the source of truth so newly saved boards appear without reboot.
// Runtime data (spiRank, pinCount) from _dbgBoards is overlaid when available.
function renderDebugBoards() {
  // Use config boards as source of truth (reflects saves immediately).
  // Overlay runtime data (spiRank, pinCount) from _dbgBoards when available.
  var cfgBoards = (_dbgCfg && _dbgCfg.boards) || [];
  var boards = cfgBoards.map(function (cb, cfgIdx) {
    var rt = null;
    for (var k = 0; k < _dbgBoards.length; k++) {
      if (_dbgBoards[k].id === cb.id) { rt = _dbgBoards[k]; break; }
    }
    return {
      id: cb.id,
      type: cb.type || '',
      bus: cb.bus || '',
      pinCount: rt ? rt.pinCount : (cb.pin_count || 0),
      spiRank: rt ? rt.spiRank : 0,
      _cfgIdx: cfgIdx
    };
  });
  var html = boards.map(function (b, i) { return renderDbgBoard(b, i); }).join('');
  document.getElementById('dbg-boards').innerHTML =
    html || '<div class="prm-info">' + t('dbg.no_boards') + '</div>';
}

// board  — entry from /api/boards: { id, type, bus, pinCount, spiRank }
// boardApiIdx — 0-based index in _dbgBoards (firmware uses boardApiIdx+1 for matching)
function renderDbgBoard(board, boardApiIdx) {
  var def = _boardTypes[board.type];
  var badge = board.spiRank > 0
    ? ' <span class="dbg-idx-badge">board\u00a0' + board.spiRank + '</span>'
    : ' <span class="dbg-idx-badge">GPIO</span>';
  var name = tbt(board.type, 'label', (def && def.label) ? def.label : board.type);

  var cfgIdx = board._cfgIdx !== undefined ? board._cfgIdx : -1;
  return '<div class="dbg-board">'
    + '<div class="dbg-board-hdr">'
    + '<span class="dbg-board-name">' + name + badge + '</span>'
    + '<div class="dbg-board-actions">'
    + '<button class="dbg-hbtn on"  onclick="dbgAll(' + boardApiIdx + ',1)">' + t('dbg.all_on') + '</button>'
    + '<button class="dbg-hbtn off" onclick="dbgAll(' + boardApiIdx + ',0)">' + t('dbg.all_off') + '</button>'
    + (def && def.rows > 0 ? '<button class="dbg-hbtn test" onclick="dbgAllTest(' + boardApiIdx + ')">' + t('dbg.all_test') + '</button>' : '')
    + (cfgIdx >= 0 ? '<button class="dbg-hbtn" onclick="openBoardEditor(' + cfgIdx + ')">' + t('be.edit') + '</button>' : '')
    + (cfgIdx >= 0 ? '<button class="dbg-hbtn off" onclick="deleteBoard(\'' + board.id.replace(/'/g, "\\'") + '\')">' + t('de.del') + '</button>' : '')
    + '</div></div>'
    + (def ? renderDipPcb(board, boardApiIdx, def) : '')
    + '</div>';
}

// Render the PCB diagram for one board.
// rows=0 boards (UART servo chains, DfAudio, I²C modules…) have no physical pins
// to display; they render a flat device list instead.
// Multi-column boards (e.g. ESP32Mini) use col:1/col:2 to arrange two pin columns per side.
function renderDipPcb(board, boardApiIdx, def) {
  if (!def.rows || def.rows === 0) {
    // Source of truth for bus devices: config (not runtime), so newly added devices appear immediately.
    var cfgDevs = (_dbgCfg && _dbgCfg.devices || []).filter(function (d) { return d.board === board.id; });
    var devs = cfgDevs.map(function (cfgDev) {
      var rt = null;
      for (var k = 0; k < _dbgDevs.length; k++) {
        if (_dbgDevs[k].id === cfgDev.id) { rt = _dbgDevs[k]; break; }
      }
      var merged = {
        id: cfgDev.id,
        type: cfgDev.type,
        board: boardApiIdx + 1,
        servoId: Array.isArray(cfgDev.wiring) ? cfgDev.wiring[0] : cfgDev.wiring,
        desired: rt ? rt.desired : 0,
        state: rt ? rt.state : 0,
        addr: cfgDev.address || 0,
        pins: rt ? (rt.pins || []) : [],
        label: cfgDev.label
      };
      if (cfgDev.positions !== undefined) merged.positions = cfgDev.positions;
      if (cfgDev.pulse_min_us !== undefined) merged.pulse_min_us = cfgDev.pulse_min_us;
      if (cfgDev.pulse_max_us !== undefined) merged.pulse_max_us = cfgDev.pulse_max_us;
      if (cfgDev.speed !== undefined) merged.speed = cfgDev.speed;
      if (cfgDev.states !== undefined) merged.states = cfgDev.states;
      if (cfgDev.neutral_us !== undefined) merged.neutral_us = cfgDev.neutral_us;
      if (cfgDev.angle_a !== undefined) merged.angle_a = cfgDev.angle_a;
      if (cfgDev.angle_b !== undefined) merged.angle_b = cfgDev.angle_b;
      _busDev[cfgDev.id] = merged; // cache for openDevEditorById
      return merged;
    });
    return '<div class="dbg-pcb">'
      + '<div class="dbg-info">' + tbt(board.type, 'desc', def.description || '') + '</div>'
      + devs.map(function (d) { return renderBusDevice(boardApiIdx, d); }).join('')
      + (def.no_devices ? '' :
        '<div class="dbg-bus-add"><button class="dbg-hbtn" onclick="openDevEditor('
        + boardApiIdx + ',1,null,SERVO_TYPES)">' + t('de.add_btn') + '</button></div>')
      + '</div>';
  }
  var allPins = def.pins || [];
  // Return the rendered HTML for all pins on a given side and column index of the DIP diagram.
  function colRow(side, col) {
    return allPins
      .filter(function (p) { return p.side === side && (p.col || 1) === col; })
      .sort(function (a, b) { return a.row - b.row; })
      .map(function (p) { return renderPin(board, boardApiIdx, p); })
      .join('');
  }
  var hasMultiCol = allPins.some(function (p) { return (p.col || 1) > 1; });
  var shortLabel = tbt(board.type, 'label', def.label || board.type);
  var usbSide = def.usb || '';
  var usbSvg = '<svg width="28" height="20" viewBox="0 0 28 20">'
    + '<rect x="1" y="1" width="26" height="18" rx="3" fill="#b0b0b0" stroke="#777" stroke-width="1.5"/>'
    + '<rect x="4" y="4" width="20" height="12" rx="2" fill="#e8e8e8" stroke="#999" stroke-width="1"/>'
    + '</svg>';
  var usbElRight = '<div class="dbg-usb-right">' + usbSvg + '<span>USB</span></div>';
  var usbElBottom = '<div class="dbg-usb-bottom">' + usbSvg + '<span>USB</span></div>';
  var dip = '<div class="dbg-dip">';
  // Wrap the chip label, attaching the USB connector SVG on the correct side when needed.
  function wrapChip(chipHtml) {
    if (usbSide === 'right') {
      return '<div class="dbg-chip-row"><div class="dbg-chip">' + chipHtml + '</div>' + usbElRight + '</div>';
    }
    return '<div class="dbg-chip">' + chipHtml + '</div>';
  }
  if (hasMultiCol) {
    // outer row first (col:2), then inner row (col:1), then chip, then inner, then outer
    dip += '<div class="dbg-col">' + colRow('left', 2) + '</div>';
    dip += '<div class="dbg-col">' + colRow('left', 1) + '</div>';
    dip += wrapChip(shortLabel);
    dip += '<div class="dbg-col">' + colRow('right', 1) + '</div>';
    dip += '<div class="dbg-col">' + colRow('right', 2) + '</div>';
  } else {
    var _conns = def.connectors || [];
    var _lcArr = _conns.filter(function (c) { return c.side === 'left'; });
    var _rcArr = _conns.filter(function (c) { return c.side === 'right'; });
    // Render one physical bus connector block (label + pin rows) for non-DIP boards.
    function renderBusConn(conn) {
      return '<div class="dbg-bus-conn">'
        + '<div class="dbg-bconn-lbl">' + conn.label + '</div>'
        + (conn.pins || []).map(function (p) {
          return '<div class="dbg-bconn-pin">'
            + '<span class="dbg-bconn-name">' + p.label + '</span>'
            + '<span class="dbg-bconn-gpio">' + p.gpio + '</span>'
            + '</div>';
        }).join('')
        + '</div>';
    }
    var _leftPinsHtml = '<div class="dbg-col">' + colRow('left', 1) + '</div>';
    if (_lcArr.length || _rcArr.length) {
      dip += '<div class="dbg-conn-row">'
        + _lcArr.map(renderBusConn).join('')
        + _leftPinsHtml
        + _rcArr.map(renderBusConn).join('')
        + '</div>';
    } else {
      dip += _leftPinsHtml;
    }
    dip += wrapChip(shortLabel);
    dip += '<div class="dbg-col">' + colRow('right', 1) + '</div>';
  }
  if (usbSide === 'bottom') dip += usbElBottom;
  dip += '</div>';
  return '<div class="dbg-pcb">' + dip + '</div>';
}

// Render one device row for a rows=0 bus board (UART servo chain, DfAudio, etc.)
function renderBusDevice(boardApiIdx, dev) {
  var isServo = SERVO_TYPES.indexOf(dev.type) >= 0;
  var isOn = dev.desired > 0;
  var sf = dev.id.replace(/'/g, "\\'");
  var html = '<div class="dbg-bus-dev">';
  html += '<span class="dbg-bus-id">' + dev.id + '</span>';
  // For servo: show bus ID from servoId field. For others: show DCC addr if set.
  var busId = isServo
    ? (dev.servoId !== undefined ? dev.servoId : '?')
    : (dev.addr > 0 ? dev.addr : null);
  if (busId !== null)
    html += '<span class="dbg-bus-addr">#' + busId + '</span>';
  html += '<button class="dbg-hbtn ' + (isOn ? 'on' : 'off') + '"'
    + ' onclick="dbgToggleDev(\'' + sf + '\',' + (isOn ? 0 : 1) + ')">'
    + (isOn ? 'ON' : 'OFF') + '</button>';
  if (isServo) {
    SERVO_STATES.forEach(function (s) {
      var action = s.v === 'REV'
        ? 'revServo(\'' + sf + '\')'
        : 'setServo(\'' + sf + '\',' + s.v + ')';
      html += '<button class="dbg-hbtn ' + s.c + '" onclick="' + action + '">' + s.l + '</button>';
    });
  }
  html += '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '"'
    + ' onclick="openDevEditorById(\'' + sf + '\',' + boardApiIdx + ',1,SERVO_TYPES)">&#9998;</button>';
  html += '<button class="dbg-del-btn" title="' + t('de.del') + '"'
    + ' onclick="deleteBusDev(\'' + sf + '\')">&#10005;</button>';
  html += '</div>';
  return html;
}

// Delete a bus-board device from config.json (UART servo chain, DfAudio, etc.).
function deleteBusDev(id) {
  if (!confirm(t('de.del_confirm', { id: id }))) return;
  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      cfg.devices = cfg.devices.filter(function (d) { return d.id !== id; });
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { _reloadAfterSave(); loadDebug(); })
    .catch(function (e) { alert(t('de.err_prefix') + e.message); });
}

// Render one pin cell in the DIP diagram.
// Pin state machine (6 states):
//   cfg-only  — device in config but firmware not restarted; shown greyed, no toggle
//   on/off    — assigned device, toggleable
//   sys       — GPIO reserved by a bus (_dbgSysPins); shown locked
//   boot      — strapping pin; shown with STRAP label
//   ro        — input-only GPIO; shown but not clickable
//   nc        — no output capability; non-connectable
//   (SPI)     — free SPI channel; toggleable via /api/test/spi
//   (GPIO)    — free GPIO output; toggleable via /api/test/gpio
// Note: _dbgSysPins is only applied to MCU boards (busType===null).
//       SPI expansion cards use logical channels 1-16 that would collide with MCU GPIO numbers.
function renderPin(board, boardApiIdx, pin) {
  var caps = pin.capabilities || [];

  if (pin.wiring === undefined) {
    var sc = caps.indexOf('gnd') >= 0 ? 'gnd'
      : caps.indexOf('power') >= 0 ? 'pwr'
        : caps.indexOf('bus_reserved') >= 0 ? 'bus-rsv'
          : 'nc';
    return '<div class="dbg-pin ' + sc + '">' + pin.label + '</div>';
  }

  var num = pin.wiring;
  var isSpi = board.spiRank > 0;
  var btDef = _boardTypes[board.type] || {};
  var isI2c = !isSpi && btDef.busType === 'i2c';
  var dev = dbgFindDev(boardApiIdx, num);
  var cls = '', onclick = '', inner = '', ledBtn = '', editBtn = '';

  // Build a small LED toggle button for a free MCU GPIO (direct hardware test).
  function mkLedBtn(gpio) {
    var ts = _dbgTest['g' + gpio] ? 1 : 0;
    return '<button class="dbg-led-btn ' + (ts ? 'on' : 'off') + '"'
      + ' onclick="event.stopPropagation();dbgTestGpio(' + gpio + ',' + (1 - ts) + ')"'
      + ' title="GPIO\u00a0' + gpio + ' direct">' + LED_ICO + '</button>';
  }

  // Build a small LED toggle button for a SPI expansion card channel (direct hardware test).
  function mkSpiLedBtn(card, ch) {
    var key = 'c' + card + '_p' + ch;
    var ts = _dbgTestSpi[key] ? 1 : 0;
    return '<button class="dbg-led-btn ' + (ts ? 'on' : 'off') + '"'
      + ' onclick="event.stopPropagation();dbgTestSpi(' + card + ',' + ch + ',' + (1 - ts) + ')"'
      + ' title="card\u00a0' + card + '\u00a0ch\u00a0' + ch + '">' + LED_ICO + '</button>';
  }

  var i2cTypeFilter = isI2c ? ',I2C_SERVO_TYPES.concat(I2C_MOTOR_TYPES)' : '';
  if (dev && dev._cfgOnly) {
    cls = 'cfg';
    var ico = ICONS[dev.type] || ICONS['_'];
    var tip = tooltip(dev.type);
    inner = '<div class="dbg-pin-ico" title="' + tip + '">' + ico + '</div>'
      + '<span class="dbg-pin-num">' + pin.label + '</span>';
    editBtn = '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '" onclick="event.stopPropagation();openDevEditorById(\'' + dev.id + '\',' + boardApiIdx + ',' + num + i2cTypeFilter + ')">✎</button>';
  } else if (dev) {
    var isDevI2cServo = I2C_SERVO_TYPES.indexOf(dev.type) >= 0;
    cls = dev.desired > 0 ? 'on' : 'off';
    if (isDevI2cServo) {
      var sc = dev.stateCount || 2;
      onclick = ' onclick="dbgCycleDev(\'' + dev.id + '\',' + dev.desired + ',' + sc + ')"';
    } else {
      var ns = dev.desired > 0 ? 0 : 1;
      onclick = ' onclick="dbgToggleDev(\'' + dev.id + '\',' + ns + ')"';
    }
    var ico = ICONS[dev.type] || ICONS['_'];
    var tip = tooltip(dev.type);
    var stateLabel = isDevI2cServo && dev.desired > 0 ? '<span class="dbg-pin-state">P' + dev.desired + '</span>' : '';
    inner = '<div class="dbg-pin-ico" title="' + tip + '">' + ico + '</div>'
      + '<span class="dbg-pin-num">' + pin.label + '</span>' + stateLabel;
    if (!isI2c) ledBtn = isSpi ? mkSpiLedBtn(board.spiRank, num) : mkLedBtn(num);
    editBtn = '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '" onclick="event.stopPropagation();openDevEditorById(\'' + dev.id + '\',' + boardApiIdx + ',' + num + i2cTypeFilter + ')">✎</button>';
  } else if (isI2c) {
    // I²C expansion channel — no GPIO test, open I2C servo editor
    inner = '<span class="dbg-pin-num">' + pin.label + '</span>';
    if (caps.indexOf('output') >= 0) {
      editBtn = '<button class="dbg-edit-btn" title="' + t('de.add_tip') + '" onclick="event.stopPropagation();openDevEditor(' + boardApiIdx + ',' + num + ',null,I2C_SERVO_TYPES.concat(I2C_MOTOR_TYPES))">+</button>';
    } else {
      cls = 'nc';
    }
  } else if (!isSpi) {
    var sysLbl = _dbgSysPins[num] || '';
    if (sysLbl) {
      cls = 'sys';
      inner = '<span class="dbg-pin-num">' + num + '</span>'
        + '<span class="dbg-pin-sys">' + sysLbl + '</span>';
    } else if (caps.indexOf('strapping') >= 0) {
      cls = 'boot';
      inner = '<span class="dbg-pin-num">' + num + '</span>'
        + '<span class="dbg-pin-sys">STRAP</span>';
    } else if (caps.length === 1 && caps[0] === 'input') {
      cls = 'ro';
      inner = '<span class="dbg-pin-num">' + num + '</span>';
    } else if (caps.indexOf('output') < 0) {
      cls = 'nc';
      inner = '<span class="dbg-pin-num">' + num + '</span>';
    } else {
      var ts2 = _dbgTest['g' + num] ? 1 : 0;
      cls = ts2 ? 'test-on' : '';
      onclick = ' onclick="dbgTestGpio(' + num + ',' + (1 - ts2) + ')"';
      inner = '<span class="dbg-pin-num">' + num + '</span>';
      ledBtn = mkLedBtn(num);
      editBtn = '<button class="dbg-edit-btn" title="' + t('de.add_tip') + '" onclick="event.stopPropagation();openDevEditor(' + boardApiIdx + ',' + num + ',null)">+</button>';
    }
  } else {
    var spiKey = 'c' + board.spiRank + '_p' + num;
    var spiTs = _dbgTestSpi[spiKey] ? 1 : 0;
    cls = spiTs ? 'test-on' : '';
    onclick = ' onclick="dbgTestSpi(' + board.spiRank + ',' + num + ',' + (1 - spiTs) + ')"';
    inner = '<span class="dbg-pin-num">' + num + '</span>';
    ledBtn = mkSpiLedBtn(board.spiRank, num);
    editBtn = '<button class="dbg-edit-btn" title="' + t('de.add_tip') + '" onclick="event.stopPropagation();openDevEditor(' + boardApiIdx + ',' + num + ',null)">+</button>';
  }

  return '<div class="dbg-pin' + (cls ? ' ' + cls : '') + '"' + onclick + '>' + inner + ledBtn + editBtn + '</div>';
}

// ── Actions ──────────────────────────────────────────────────────────

// Toggle a device on/off from the boards tab (uses /api/switch, then reloads).
function dbgToggleDev(id, on) {
  post('/api/switch', { id: id, on: !!on })
    .then(function () { loadDebug(); })
    .catch(function (e) { console.error('dbgToggleDev', e); });
}

// Cycle a multi-state device (e.g. PCA9685Servo) through its states on each click.
// desired=current state, stateCount=total states (0=STOP + N positions).
function dbgCycleDev(id, desired, stateCount) {
  var next = (desired + 1) % stateCount;
  post('/api/device', { id: id, state: next })
    .then(function () { loadDebug(); })
    .catch(function (e) { console.error('dbgCycleDev', e); });
}

// Apply a UI theme (night | amber | signal); persists choice in localStorage.
function setTheme(name) {
  document.body.classList.remove('th-amber', 'th-signal');
  if (name !== 'night') document.body.classList.add('th-' + name);
  localStorage.setItem('mrj-theme', name);
  document.querySelectorAll('.theme-dot').forEach(function (b) {
    var match = b.classList.contains('theme-dot-' + name);
    b.classList.toggle('active', match);
  });
}

// Drive a single GPIO directly via /api/test/gpio (for hardware testing, bypasses device layer).
function dbgTestGpio(pin, state) {
  fetch('/api/test/gpio', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ pin: pin, state: state })
  })
    .then(function () {
      _dbgTest['g' + pin] = state;
      renderDebugBoards();  // fast local re-render
    })
    .catch(function (e) { console.error('dbgTestGpio', e); });
}

// Drive a single SPI expansion card channel directly via /api/test/spi (hardware test).
function dbgTestSpi(card, channel, state) {
  fetch('/api/test/spi', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ card: card, channel: channel, state: state })
  })
    .then(function () {
      _dbgTestSpi['c' + card + '_p' + channel] = state;
      renderDebugBoards();  // fast local re-render
    })
    .catch(function (e) { console.error('dbgTestSpi', e); });
}

// Clear client-side SPI test state for a card.
function dbgClearSpiTest(card, pinCount) {
  for (var p = 1; p <= pinCount; p++) delete _dbgTestSpi['c' + card + '_p' + p];
}

// All-on / all-off for one board.
// Active test pins (/api/test/gpio or /api/test/spi) must be explicitly reset to 0
// before /api/all — otherwise firmware _buf still holds them lit after the call.
function dbgAll(boardApiIdx, state) {
  var board = _dbgBoards[boardApiIdx];
  var clearCalls = [];
  if (board) {
    if (board.spiRank > 0) {
      // Explicitly reset every active test pin in _buf before /api/all
      var card = board.spiRank;
      var pinCount = board.pinCount || 16;
      for (var p = 1; p <= pinCount; p++) {
        if (_dbgTestSpi['c' + card + '_p' + p]) {
          clearCalls.push(fetch('/api/test/spi', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ card: card, channel: p, state: 0 })
          }));
        }
      }
      dbgClearSpiTest(card, pinCount);
    } else {
      var def = _boardTypes[board.type];
      var gpins = (def && def.pins) ? def.pins : [];
      gpins.forEach(function (pin) {
        if (pin.wiring !== undefined && _dbgTest['g' + pin.wiring]) {
          clearCalls.push(fetch('/api/test/gpio', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ pin: pin.wiring, state: 0 })
          }));
          delete _dbgTest['g' + pin.wiring];
        }
      });
    }
  }
  Promise.all(clearCalls).then(function () {
    post('/api/all', { state: state, board: boardApiIdx + 1 })
      .then(function () { loadDebug(); });
  });
}

// ALL test: turn off all devices on the card, then light every testable pin.
function dbgAllTest(boardApiIdx) {
  var board = _dbgBoards[boardApiIdx];
  if (!board) return;
  post('/api/all', { state: 0, board: boardApiIdx + 1 })
    .then(function () {
      var calls = [];
      if (board.spiRank > 0) {
        // SPI board: test all channels 1..pinCount
        var card = board.spiRank;
        var pinCount = board.pinCount || 16;
        for (var p = 1; p <= pinCount; p++) {
          _dbgTestSpi['c' + card + '_p' + p] = 1;
          calls.push(fetch('/api/test/spi', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ card: card, channel: p, state: 1 })
          }));
        }
      } else {
        // GPIO board: test all output pins that are not system pins
        var def = _boardTypes[board.type];
        var pins = (def && def.pins) ? def.pins : [];
        pins.forEach(function (pin) {
          if (pin.wiring === undefined) return;
          if (_dbgSysPins[pin.wiring]) return;
          var caps = pin.capabilities || [];
          if (caps.indexOf('output') < 0) return;
          _dbgTest['g' + pin.wiring] = 1;
          calls.push(fetch('/api/test/gpio', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ pin: pin.wiring, state: 1 })
          }));
        });
      }
      return Promise.all(calls);
    })
    .then(function () { renderDebugBoards(); })
    .catch(function (e) { console.error('dbgAllTest', e); });
}

/* ── About ──────────────────────────────────────────────────────────── */

// Zero-pad (duplicate of the one in API calls — kept here to avoid cross-section dependency).
function pad2(n) { return n < 10 ? '0' + n : String(n); }

var _featTipEl = null;

// Show a positioned tooltip for a feature badge; auto-flips above if it would go off-screen.
function showFeatTip(e, text) {
  e.stopPropagation();
  if (!_featTipEl) _featTipEl = document.getElementById('abt-tooltip');
  _featTipEl.textContent = text;
  _featTipEl.style.display = 'block';
  var r = e.target.getBoundingClientRect();
  var tw = _featTipEl.offsetWidth;
  var left = Math.min(r.left, window.innerWidth - tw - 8);
  var top = r.bottom + 6;
  if (top + _featTipEl.offsetHeight > window.innerHeight - 8)
    top = r.top - _featTipEl.offsetHeight - 6;
  _featTipEl.style.left = Math.max(8, left) + 'px';
  _featTipEl.style.top = top + 'px';
}
// Hide the feature tooltip (bound to document click to auto-dismiss).
function hideFeatTip() {
  if (_featTipEl) _featTipEl.style.display = 'none';
}
document.addEventListener('click', hideFeatTip);

// Format a byte count as a human-readable string (B / KB / MB).
function fmtBytes(b) {
  if (b === undefined || b === null) return '\u2014';
  if (b >= 1048576) return (b / 1048576).toFixed(1) + '\u00a0MB';
  if (b >= 1024) return (b / 1024).toFixed(1) + '\u00a0KB';
  return b + '\u00a0B';
}

// Format an uptime in seconds as "Dj HH:MM:SS" (day part omitted if d=0).
function fmtUptime(s) {
  var d = Math.floor(s / 86400);
  var h = Math.floor((s % 86400) / 3600);
  var m = Math.floor((s % 3600) / 60);
  var sec = s % 60;
  var str = pad2(h) + ':' + pad2(m) + ':' + pad2(sec);
  return d > 0 ? d + t('abt.day_unit') + '\u00a0' + str : str;
}

// Render a titled status card with label/value rows and optional percentage progress bars.
// bar ≥ 85 → red (crit), ≥ 65 → yellow (warn).
function abtCard(title, rows) {
  return '<div class="abt-card">'
    + '<div class="abt-card-title">' + title + '</div>'
    + rows.map(function (r) {
      var barHtml = '';
      if (r.bar !== undefined) {
        var cls = r.bar > 85 ? ' crit' : r.bar > 65 ? ' warn' : '';
        barHtml = '<div class="abt-bar-wrap"><div class="abt-bar' + cls
          + '" style="width:' + r.bar + '%"></div></div>';
      }
      return '<div class="abt-row">'
        + '<span class="abt-label">' + r.label + '</span>'
        + '<span class="abt-value">' + r.value + '</span>'
        + '</div>' + barHtml;
    }).join('')
    + '</div>';
}

// Fetch /api/status and render the about page.
function loadAbout() {
  var grid = document.getElementById('abt-grid');
  grid.innerHTML = '<div class="prm-info">' + t('prm.loading') + '</div>';

  fetch('/api/status')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (s) { renderAbout(s); })
    .catch(function (err) {
      grid.innerHTML = '<div class="prm-info">' + t('prm.load_err') + ': ' + err.message + '</div>';
    });
}

// Build the about page from a /api/status response object.
// Cards shown conditionally: filesystem only if s.fs_total present, WiFi only if s.wifi_ssid present, etc.
function renderAbout(s) {
  var html = '';

  // Firmware
  html += abtCard(t('abt.firmware'), [
    { label: t('abt.version'), value: s.version || '\u2014' },
    { label: t('abt.build'), value: s.build_date || '\u2014' },
    { label: t('abt.env'), value: s.env || '\u2014' },
  ]);

  // System
  var chip = (s.chip || 'ESP32') + ' rev.\u00a0' + (s.chip_rev !== undefined ? s.chip_rev : '?');
  html += abtCard(t('abt.system'), [
    { label: t('abt.chip'), value: chip },
    { label: t('abt.cpu_freq'), value: (s.cpu_mhz || '\u2014') + '\u00a0MHz' },
    { label: t('abt.uptime'), value: fmtUptime(s.uptime_s || 0) },
  ]);

  // Memory
  var heapPct = s.heap_total ? Math.round((1 - s.heap_free / s.heap_total) * 100) : 0;
  html += abtCard(t('abt.memory'), [
    {
      label: t('abt.heap_free'),
      value: fmtBytes(s.heap_free) + '\u00a0/\u00a0' + fmtBytes(s.heap_total),
      bar: heapPct
    },
    { label: t('abt.heap_min'), value: fmtBytes(s.heap_min) },
  ]);

  // Flash
  var fwUsed = s.sketch_size || 0;
  var fwTotal = fwUsed + (s.sketch_free || 0);
  var fwPct = fwTotal ? Math.round(fwUsed / fwTotal * 100) : 0;
  html += abtCard(t('abt.flash'), [
    {
      label: t('abt.firmware_size'),
      value: fmtBytes(fwUsed) + '\u00a0/\u00a0' + fmtBytes(fwTotal),
      bar: fwPct
    },
  ]);

  // Filesystem
  if (s.fs_total !== undefined) {
    var fsPct = s.fs_total ? Math.round(s.fs_used / s.fs_total * 100) : 0;
    html += abtCard(t('abt.fs'), [
      {
        label: 'LittleFS',
        value: fmtBytes(s.fs_used) + '\u00a0/\u00a0' + fmtBytes(s.fs_total),
        bar: fsPct
      },
    ]);
  }

  // Configuration
  if (s.devices !== undefined) {
    var devMax = s.devices_max || 0;
    var devPct = devMax ? Math.round(s.devices / devMax * 100) : 0;
    html += abtCard(t('abt.config'), [
      {
        label: t('abt.devices'),
        value: s.devices + '\u00a0/\u00a0' + devMax,
        bar: devPct
      },
    ]);
  }

  // Temperature
  if (s.temp_c !== undefined) {
    var tempPct = Math.min(100, Math.max(0, Math.round((s.temp_c - 20) * 100 / 80)));
    html += abtCard(t('abt.temp'), [
      {
        label: 'CPU',
        value: s.temp_c.toFixed(1) + '\u00a0\u00b0C',
        bar: tempPct
      },
    ]);
  }

  // WiFi
  if (s.wifi_ssid !== undefined) {
    var rssi = s.wifi_rssi || 0;
    var rssiPct = Math.min(100, Math.max(0, Math.round((rssi + 100) * 2)));
    var rssiLabel = rssi >= -60 ? '\uD83D\uDFE2' : rssi >= -75 ? '\uD83D\uDFE1' : '\uD83D\uDD34';
    html += abtCard(t('abt.wifi'), [
      { label: t('abt.wifi_ssid'), value: s.wifi_ssid || '\u2014' },
      { label: t('abt.wifi_rssi'), value: rssiLabel + '\u00a0' + rssi + '\u00a0dBm', bar: rssiPct },
      { label: t('abt.wifi_mac'), value: s.wifi_mac || '\u2014' },
    ]);
  }

  // Libraries
  if (s.libs) {
    var libRows = Object.keys(s.libs).map(function (k) {
      return { label: k, value: s.libs[k] || '\u2014' };
    });
    html += abtCard(t('abt.libs'), libRows);
  }

  // Features
  if (s.features) {
    var FEAT_LABELS = {
      api: 'API', audio: 'Audio', config: 'Config', dcc: 'DCC',
      lobot_servo: 'Lobot Servo', lx16a_servo: 'LX-16A Servo',
      i2c: 'I²C', oled: 'OLED', spi: 'SPI', webui: 'WebUI', wifi: 'WiFi'
    };
    var badges = '';
    Object.keys(FEAT_LABELS).forEach(function (k) {
      if (s.features[k] === undefined) return;
      var on = s.features[k];
      var tip = t('abt.feat.' + k).replace(/'/g, '&#39;');
      badges += '<span class="abt-feat' + (on ? ' on' : ' off') + '" onclick="showFeatTip(event,\'' + tip + '\')">' + FEAT_LABELS[k] + '</span>';
    });
    html += '<div class="abt-card"><div class="abt-card-title">' + t('abt.features') + '</div>'
      + '<div class="abt-feat-row">' + badges + '</div></div>';
  }

  document.getElementById('abt-grid').innerHTML = html;
}

/* ── Params ─────────────────────────────────────────────────────────── */
var _pollTimer = null;

// Apply the poll interval from the settings input; resets the running interval timer.
// Minimum value is 500 ms to avoid flooding the ESP32.
function savePollInterval() {
  var v = parseInt(document.getElementById('prm-poll').value, 10);
  if (v >= 500) {
    POLL = v;
    localStorage.setItem('poll', String(v));
    if (_pollTimer) clearInterval(_pollTimer);
    _pollTimer = setInterval(poll, POLL);
  }
}

// Placeholder — params are now stored in localStorage only, nothing to load from server.
function loadParams() {
  // Nothing to load from server anymore — only local UI settings
}


/* ── Device editor ──────────────────────────────────────────────────── */

var _deEditId = null;    // id of the device currently being edited, null = new
var _deFixedPin = undefined;          // pin/channel locked from context (pin click), undefined = free
var _deFixedBoardApiIdx = undefined;  // board index locked from context (I2C boards)

// Opens the device editor for an existing device, looking up the best available data source.
// Bus devices come from _busDev (config-sourced, has servoId); GPIO devices come from _dbgDevs.
function openDevEditorById(id, boardApiIdx, pin, typeFilter) {
  // For bus devices, always prefer the config-sourced merged dev (has servoId from wiring).
  // Overlay runtime state (desired/state) if the device is also running in firmware.
  if (_busDev[id]) {
    var rt = null;
    for (var i = 0; i < _dbgDevs.length; i++) {
      if (_dbgDevs[i].id === id) { rt = _dbgDevs[i]; break; }
    }
    var bd = _busDev[id];
    var merged = {
      id: bd.id,
      type: bd.type,
      board: bd.board,
      servoId: bd.servoId,
      desired: rt ? rt.desired : bd.desired,
      state: rt ? rt.state : bd.state,
      addr: bd.addr,
      pins: bd.pins
    };
    openDevEditor(boardApiIdx, pin, merged, typeFilter);
    return;
  }
  // Non-bus devices: use runtime dev, merging config-only fields (angle_a, angle_b, etc.)
  for (var j = 0; j < _dbgDevs.length; j++) {
    if (_dbgDevs[j].id === id) {
      var rtDev = _dbgDevs[j];
      var cfgLookup = null;
      if (_dbgCfg && _dbgCfg.devices) {
        for (var k = 0; k < _dbgCfg.devices.length; k++) {
          if (_dbgCfg.devices[k].id === id) { cfgLookup = _dbgCfg.devices[k]; break; }
        }
      }
      if (cfgLookup && (cfgLookup.angle_a !== undefined || cfgLookup.angle_b !== undefined
        || cfgLookup.positions !== undefined || cfgLookup.speed !== undefined
        || cfgLookup.states !== undefined || cfgLookup.neutral_us !== undefined)) {
        var mDev = {
          id: rtDev.id, type: rtDev.type, board: rtDev.board,
          desired: rtDev.desired, state: rtDev.state, addr: rtDev.addr,
          pins: rtDev.pins, label: rtDev.label
        };
        if (cfgLookup.angle_a !== undefined) mDev.angle_a = cfgLookup.angle_a;
        if (cfgLookup.angle_b !== undefined) mDev.angle_b = cfgLookup.angle_b;
        if (cfgLookup.positions !== undefined) mDev.positions = cfgLookup.positions;
        if (cfgLookup.pulse_min_us !== undefined) mDev.pulse_min_us = cfgLookup.pulse_min_us;
        if (cfgLookup.pulse_max_us !== undefined) mDev.pulse_max_us = cfgLookup.pulse_max_us;
        if (cfgLookup.speed !== undefined) mDev.speed = cfgLookup.speed;
        if (cfgLookup.states !== undefined) mDev.states = cfgLookup.states;
        if (cfgLookup.neutral_us !== undefined) mDev.neutral_us = cfgLookup.neutral_us;
        openDevEditor(boardApiIdx, pin, mDev, typeFilter);
      } else {
        openDevEditor(boardApiIdx, pin, rtDev, typeFilter);
      }
      return;
    }
  }
  // Device not in runtime — look up in config (cfg-only: firmware not restarted after save).
  if (_dbgCfg && _dbgCfg.devices) {
    for (var m = 0; m < _dbgCfg.devices.length; m++) {
      var cfgDev = _dbgCfg.devices[m];
      if (cfgDev.id === id) {
        var cfgOnlyDev = {
          id: cfgDev.id, type: cfgDev.type,
          board: boardApiIdx + 1,
          desired: 0, state: 0,
          addr: cfgDev.address || 0,
          pins: [pin], label: cfgDev.label
        };
        if (cfgDev.positions !== undefined) cfgOnlyDev.positions = cfgDev.positions;
        if (cfgDev.pulse_min_us !== undefined) cfgOnlyDev.pulse_min_us = cfgDev.pulse_min_us;
        if (cfgDev.pulse_max_us !== undefined) cfgOnlyDev.pulse_max_us = cfgDev.pulse_max_us;
        if (cfgDev.speed !== undefined) cfgOnlyDev.speed = cfgDev.speed;
        if (cfgDev.states !== undefined) cfgOnlyDev.states = cfgDev.states;
        if (cfgDev.neutral_us !== undefined) cfgOnlyDev.neutral_us = cfgDev.neutral_us;
        if (cfgDev.angle_a !== undefined) cfgOnlyDev.angle_a = cfgDev.angle_a;
        if (cfgDev.angle_b !== undefined) cfgOnlyDev.angle_b = cfgDev.angle_b;
        openDevEditor(boardApiIdx, pin, cfgOnlyDev, typeFilter);
        return;
      }
    }
  }
  openDevEditor(boardApiIdx, pin, null, typeFilter);
}

// Open the device add/edit modal.
// prefillPin pre-selects the wiring pin (used when clicking a free pin cell).
// dev is the existing device data (null for new); typeFilter restricts the type dropdown.
function openDevEditor(boardApiIdx, prefillPin, dev, typeFilter) {
  _deEditId = dev ? dev.id : null;
  _deFixedPin = prefillPin;
  _deFixedBoardApiIdx = boardApiIdx;

  // Type select — filtered if typeFilter provided (e.g. SERVO_TYPES for bus boards)
  var typeEl = document.getElementById('de-type');
  var typeList = typeFilter || Object.keys(_deviceTypes);
  typeEl.innerHTML = typeList.map(function (tp) {
    return '<option value="' + tp + '">' + tp + ' — ' + tooltip(tp) + '</option>';
  }).join('');
  // Lock type when there is only one option (e.g. PCA9685Servo on an I²C board)
  typeEl.disabled = typeList.length === 1;

  // Board select
  var boardEl = document.getElementById('de-board');
  boardEl.innerHTML = _dbgBoards.map(function (b, i) {
    var badge = b.spiRank > 0 ? 'SPI board\u00a0' + b.spiRank : 'GPIO';
    return '<option value="' + i + '">' + b.id + ' (' + badge + ')</option>';
  }).join('');

  if (dev) {
    document.getElementById('de-title').textContent = t('de.edit_prefix') + dev.id;
    document.getElementById('de-id').value = dev.id;
    typeEl.value = dev.type;
    // dev.board may be a string ID (from config) or a 1-based integer (from runtime API).
    // Resolve to 0-based index in _dbgBoards.
    if (typeof dev.board === 'number') {
      boardEl.value = dev.board - 1;
    } else {
      var bIdx = -1;
      for (var bi = 0; bi < _dbgBoards.length; bi++) {
        if (_dbgBoards[bi].id === dev.board) { bIdx = bi; break; }
      }
      boardEl.value = bIdx >= 0 ? bIdx : (boardApiIdx !== undefined ? boardApiIdx : 0);
    }
    document.getElementById('de-addr').value = dev.addr > 0 ? dev.addr : '';
    document.getElementById('de-defstate').value = dev.desired > 0 ? 'on' : '';
    document.getElementById('de-del-btn').style.display = '';
  } else {
    document.getElementById('de-title').textContent = t('de.new');
    document.getElementById('de-id').value = '';
    if (boardApiIdx !== undefined) boardEl.value = boardApiIdx;
    document.getElementById('de-addr').value = '';
    document.getElementById('de-defstate').value = '';
    document.getElementById('de-del-btn').style.display = 'none';
  }

  deUpdateWiring(prefillPin, dev);
  deUpdateAddrLabel();
  deStatus('', '');
  document.getElementById('de-save-btn').disabled = false;

  document.getElementById('de-overlay').style.display = 'block';
  document.getElementById('de-modal').style.display = 'flex';
  applyLang();
}

// Close the device editor modal without saving.
function closeDevEditor() {
  document.getElementById('de-overlay').style.display = 'none';
  document.getElementById('de-modal').style.display = 'none';
}

// Update addr/board field visibility and labels based on the currently selected device type.
// Servo devices (UART): board selector hidden (bus board is implicit from the chain).
// I2C servo devices: board selector hidden (board and channel are fixed from pin click context).
function deUpdateAddrLabel() {
  var type = document.getElementById('de-type').value;
  var isServo = SERVO_TYPES.indexOf(type) >= 0;
  var isI2cServo = I2C_SERVO_TYPES.indexOf(type) >= 0;
  var isI2cMotor = I2C_MOTOR_TYPES.indexOf(type) >= 0;
  // Board field: hide for UART servos and I2C boards (board is implicit from context)
  var boardField = document.getElementById('de-board-field');
  if (boardField) boardField.style.display = (isServo || isI2cServo || isI2cMotor) ? 'none' : '';
  // Addr field: always show — DCC address for both LED devices and servos
  var addrField = document.querySelector('#de-modal #de-addr');
  var addrFieldRow = addrField ? addrField.closest('.de-field') : null;
  if (addrFieldRow) addrFieldRow.style.display = '';
  var lbl = document.querySelector('#de-modal .de-addr-lbl');
  var hint = document.querySelector('#de-modal .de-addr-hint');
  if (lbl) lbl.textContent = t('de.lbl_addr');
  if (hint) hint.textContent = t('de.lbl_addr_hint');
  if (addrField) { addrField.min = 1; addrField.max = 10239; }
}

// Rebuild the wiring input(s) for the currently selected device type.
// UART servo: free-text number input (bus ID 1-253).
// I2C servo (PCA9685): channel is fixed from pin-click context — shown as a badge.
// GPIO/SPI: <select> dropdown filtered to available output pins.
// Called on modal open (prefillPin/dev provided) and on type/board change (no args).
function deUpdateWiring(prefillPin, dev) {
  var type = document.getElementById('de-type').value;
  var isServo = SERVO_TYPES.indexOf(type) >= 0;
  var isI2cServo = I2C_SERVO_TYPES.indexOf(type) >= 0;
  var isI2cMotor = I2C_MOTOR_TYPES.indexOf(type) >= 0;
  var count = (_deviceTypes[type] || {}).wires !== undefined ? (_deviceTypes[type] || {}).wires : 1;
  var grp = document.getElementById('de-wiring-grp');
  var extraGrp = document.getElementById('de-extra-grp');

  function makePosRow(lbl, ang, dur, easeOut) {
    return '<div class="de-pos-row">'
      + '<input type="text" class="de-pos-lbl" placeholder="Label" value="' + (lbl || '') + '">'
      + '<input type="number" class="de-pos-ang" min="-90" max="90" placeholder="Angle°" value="' + (ang !== undefined ? ang : 0) + '">'
      + '<input type="number" class="de-pos-dur" min="100" max="30000" placeholder="Durée ms" value="' + (dur || 2000) + '">'
      + '<label title="Ralentissement en fin de course (porte, barrière)" style="font-size:.8rem;white-space:nowrap">'
      + '<input type="checkbox" class="de-pos-ease"' + (easeOut ? ' checked' : '') + '> ↘ ease</label>'
      + '<button type="button" class="de-pos-del" onclick="deRemovePosition(this)">×</button>'
      + '</div>';
  }

  function makeMotorStateRow(lbl, spd, dur, rampUp, rampDown) {
    return '<div class="de-pos-row">'
      + '<input type="text" class="de-mst-lbl" placeholder="' + t('de.mst_lbl_ph') + '" value="' + (lbl || '') + '" style="width:5rem">'
      + '<input type="number" class="de-mst-spd" min="-100" max="100" value="' + (spd !== undefined ? spd : 50) + '" style="width:4rem" title="' + t('de.mst_spd_tip') + '">'
      + '<input type="number" class="de-mst-dur" min="0" max="300000" value="' + (dur !== undefined ? dur : 0) + '" style="width:5rem" title="' + t('de.mst_dur_tip') + '">'
      + '<input type="number" class="de-mst-up"  min="0" max="30000"  value="' + (rampUp || 0) + '" style="width:4rem" title="' + t('de.mst_up_tip') + '">'
      + '<input type="number" class="de-mst-dn"  min="0" max="30000"  value="' + (rampDown || 0) + '" style="width:4rem" title="' + t('de.mst_dn_tip') + '">'
      + '<button type="button" class="de-pos-del" onclick="deRemoveMotorState(this)">×</button>'
      + '</div>';
  }

  function renderExtraGrp() {
    if (!extraGrp) return;
    if (isI2cServo) {
      var positions = (dev && dev.positions) || [];
      var rowsHtml = positions.map(function (p) {
        return makePosRow(p.label, p.angle, p.duration_ms, p.ease_out);
      }).join('');
      if (!rowsHtml) rowsHtml = makePosRow('Pos 1', 90, 2000); // default row
      var pMin = (dev && dev.pulse_min_us !== undefined) ? dev.pulse_min_us : 1000;
      var pMax = (dev && dev.pulse_max_us !== undefined) ? dev.pulse_max_us : 2000;
      extraGrp.innerHTML = '<div class="de-field"><label>Positions</label>'
        + '<div id="de-positions-list">' + rowsHtml + '</div>'
        + '<button type="button" class="btn de-pos-add" onclick="deAddPosition()">+ Position</button>'
        + '</div>'
        + '<div class="de-field"><label>Calibration PWM (µs)</label>'
        + '<div style="display:flex;gap:.5rem;align-items:center">'
        + '<span style="font-size:.8rem">−90°</span>'
        + '<input type="number" id="de-pulse-min" min="500" max="2500" value="' + pMin + '" style="width:5rem">'
        + '<span style="font-size:.8rem">+90°</span>'
        + '<input type="number" id="de-pulse-max" min="500" max="2500" value="' + pMax + '" style="width:5rem">'
        + '<span style="font-size:.75rem;color:var(--c-muted)">SG90: 1000/2000 · ext: 500/2500</span>'
        + '</div></div>';
    } else if (isI2cMotor) {
      var motorStates = (dev && dev.states) || [];
      // Backward compat: old config with just "speed" → show as single state
      if (motorStates.length === 0 && dev && dev.speed !== undefined) {
        motorStates = [{ speed: dev.speed, duration_ms: 0, ramp_up_ms: 0, ramp_down_ms: 0 }];
      }
      var mRowsHtml = motorStates.map(function (s) {
        return makeMotorStateRow(s.label, s.speed, s.duration_ms, s.ramp_up_ms, s.ramp_down_ms);
      }).join('');
      if (!mRowsHtml) mRowsHtml = makeMotorStateRow('', 50, 0, 0, 0);
      var neu = (dev && dev.neutral_us !== undefined) ? dev.neutral_us : 1500;
      extraGrp.innerHTML = '<div class="de-field"><label>États moteur</label>'
        + '<div style="display:flex;gap:.3rem;font-size:.75rem;margin-bottom:.2rem;color:var(--c-muted)">'
        + '<span style="width:5rem">Label</span>'
        + '<span style="width:4rem">Vit.</span>'
        + '<span style="width:5rem">Durée ms</span>'
        + '<span style="width:4rem">↑ ms</span>'
        + '<span style="width:4rem">↓ ms</span>'
        + '</div>'
        + '<div id="de-motor-states-list">' + mRowsHtml + '</div>'
        + '<button type="button" class="btn de-pos-add" onclick="deAddMotorState()">' + t('de.mst_add_btn') + '</button>'
        + '</div>'
        + '<div class="de-field">'
        + '<label>' + t('de.mst_neutral_lbl') + '</label>'
        + '<div style="display:flex;gap:.5rem;align-items:center">'
        + '<input type="number" id="de-motor-neutral" min="1000" max="2000" value="' + neu + '" style="width:5rem">'
        + '<span style="font-size:.75rem;color:var(--c-muted)">' + t('de.mst_neutral_hint') + '</span>'
        + '</div></div>';
    } else {
      extraGrp.innerHTML = '';
    }
  }

  if (count === 0) { grp.innerHTML = ''; renderExtraGrp(); return; }

  // I2C servo/motor: channel locked from the pin that was clicked — no selector needed
  if ((isI2cServo || isI2cMotor) && _deFixedPin !== undefined) {
    grp.innerHTML = '<div class="de-field"><label>' + t('de.lbl_wiring') + '</label>'
      + '<div class="de-wiring-row"><span class="de-wiring-fixed">CH ' + _deFixedPin + '</span>'
      + '<input type="hidden" class="de-w" value="' + _deFixedPin + '"></div></div>';
    renderExtraGrp();
    deUpdateIdPlaceholder();
    return;
  }

  // Preserve currently displayed values when called from onchange (no args)
  var existingInputs = grp.querySelectorAll('.de-w');
  var existingVals = [];
  existingInputs.forEach(function (inp) {
    var v = parseInt(inp.value, 10);
    if (!isNaN(v)) existingVals.push(v);
  });

  var pins;
  if (dev) {
    if (isServo) {
      // Bus ID: prefer servoId (from firmware), else fall back to pins[0] if valid (not 255 sentinel)
      var sid = dev.servoId !== undefined ? dev.servoId
        : (dev.pins && dev.pins[0] < 254 ? dev.pins[0] : undefined);
      pins = sid !== undefined ? [sid] : [];
    } else {
      pins = dev.pins || [];
    }
  } else if (prefillPin !== undefined && !isServo) {
    pins = [prefillPin]; // GPIO prefill only for non-servo
  } else {
    pins = existingVals; // preserve on type change
  }

  var wiringLabel = isServo ? t('de.lbl_wiring_servo') : t('de.lbl_wiring');
  var html = '<div class="de-field"><label>' + wiringLabel + '</label><div class="de-wiring-row">';

  if (isServo) {
    for (var i = 0; i < count; i++) {
      html += '<input type="number" class="de-w" min="1" max="253"'
        + ' placeholder="ID' + (count > 1 ? '\u00a0' + (i + 1) : '') + '"'
        + ' value="' + (pins[i] !== undefined ? pins[i] : '') + '"'
        + ' oninput="deUpdateIdPlaceholder()">';
    }
  } else {
    // Build available GPIO options from board_types, filtered and sorted alphabetically
    var boardIdx2 = parseInt(document.getElementById('de-board').value, 10);
    var board2 = _dbgBoards[boardIdx2];
    var availOpts = [];
    if (board2) {
      var bt2 = _boardTypes[board2.type];
      // Collect used pins from config (source of truth) and sys_pins
      var usedPins = {};
      // _dbgSysPins are MCU GPIO numbers — skip for SPI/bus expansion cards
      // whose logical pin numbers (1-16) would collide with MCU GPIO numbers
      if (!(bt2 && bt2.busType)) {
        Object.keys(_dbgSysPins || {}).forEach(function (g) { usedPins[parseInt(g)] = true; });
      }
      var cfgBoard2Id = _dbgCfg && _dbgCfg.boards && _dbgCfg.boards[boardIdx2]
        ? _dbgCfg.boards[boardIdx2].id : (board2.id || '');
      ((_dbgCfg && _dbgCfg.devices) || []).forEach(function (d) {
        if (d.board !== cfgBoard2Id || d.id === _deEditId) return;
        var w = d.wiring;
        (Array.isArray(w) ? w : [w]).forEach(function (p) { if (p !== undefined) usedPins[parseInt(p)] = true; });
      });
      if (bt2 && bt2.pins) {
        bt2.pins.forEach(function (p) {
          if (p.wiring === undefined) return;
          var caps = p.capabilities || [];
          if (caps.indexOf('output') < 0) return; // must be driveable
          availOpts.push({ val: p.wiring, label: p.label, used: !!usedPins[p.wiring] });
        });
        availOpts.sort(function (a, b) { return a.val - b.val; });
      }
    }
    for (var i = 0; i < count; i++) {
      var curVal = pins[i] !== undefined ? pins[i] : '';
      var selLabel = count > 1 ? ' (' + (i + 1) + ')' : '';
      html += '<select class="de-w" onchange="deUpdateIdPlaceholder()">';
      html += '<option value="">— pin' + selLabel + ' —</option>';
      availOpts.forEach(function (o) {
        if (o.used && o.val !== curVal) return;
        var sel = (o.val === curVal) ? ' selected' : '';
        html += '<option value="' + o.val + '"' + sel + '>' + o.val + '</option>';
      });
      html += '</select>';
    }
  }

  html += '</div></div>';
  grp.innerHTML = html;
  renderExtraGrp();
  deUpdateIdPlaceholder();
}

// Add a new (empty) position row to the positions list editor.
function deAddPosition() {
  var list = document.getElementById('de-positions-list');
  if (!list) return;
  var n = list.querySelectorAll('.de-pos-row').length + 1;
  var row = document.createElement('div');
  row.className = 'de-pos-row';
  row.innerHTML = '<input type="text" class="de-pos-lbl" placeholder="Label" value="Pos ' + n + '">'
    + '<input type="number" class="de-pos-ang" min="-90" max="90" placeholder="Angle°" value="0">'
    + '<input type="number" class="de-pos-dur" min="100" max="30000" placeholder="Durée ms" value="2000">'
    + '<label title="Ralentissement en fin de course (porte, barrière)" style="font-size:.8rem;white-space:nowrap">'
    + '<input type="checkbox" class="de-pos-ease"> ↘ ease</label>'
    + '<button type="button" class="de-pos-del" onclick="deRemovePosition(this)">×</button>';
  list.appendChild(row);
}

// Remove a position row (called from the × button inside the row).
function deRemovePosition(btn) {
  var row = btn.parentElement;
  if (row && row.parentElement) row.parentElement.removeChild(row);
}

// Add a new (empty) motor state row to the motor states list editor.
function deAddMotorState() {
  var list = document.getElementById('de-motor-states-list');
  if (!list) return;
  var row = document.createElement('div');
  row.className = 'de-pos-row';
  row.innerHTML = '<input type="text" class="de-mst-lbl" placeholder="' + t('de.mst_lbl_ph') + '" style="width:5rem">'
    + '<input type="number" class="de-mst-spd" min="-100" max="100" value="50" style="width:4rem" title="' + t('de.mst_spd_tip') + '">'
    + '<input type="number" class="de-mst-dur" min="0" max="300000" value="0" style="width:5rem" title="' + t('de.mst_dur_tip') + '">'
    + '<input type="number" class="de-mst-up"  min="0" max="30000"  value="0" style="width:4rem" title="' + t('de.mst_up_tip') + '">'
    + '<input type="number" class="de-mst-dn"  min="0" max="30000"  value="0" style="width:4rem" title="' + t('de.mst_dn_tip') + '">'
    + '<button type="button" class="de-pos-del" onclick="deRemoveMotorState(this)">×</button>';
  list.appendChild(row);
}

// Remove a motor state row (called from the × button inside the row).
function deRemoveMotorState(btn) {
  var row = btn.parentElement;
  if (row && row.parentElement) row.parentElement.removeChild(row);
}

// Generate a suggested device ID from the type name and first pin (shown as placeholder when id is empty).
function deUpdateIdPlaceholder() {
  var idEl = document.getElementById('de-id');
  if (!idEl || idEl.value.trim()) return;
  var type = (document.getElementById('de-type') || {}).value || '';
  var count = (_deviceTypes[type] || {}).wires !== undefined ? (_deviceTypes[type] || {}).wires : 1;
  var firstPinEl = document.querySelector('.de-w');
  var firstPin = (count > 0 && firstPinEl) ? (parseInt(firstPinEl.value, 10) || '') : '';
  var shortType = type.replace(/^MrJDB/, '').toLowerCase().replace(/[^a-z0-9]/g, '');
  idEl.placeholder = shortType ? shortType + (firstPin !== '' ? firstPin : '') : 'auto';
}

// Show or hide the device editor inline status message (cls: 'ok' | 'err').
function deStatus(msg, cls) {
  var el = document.getElementById('de-status');
  el.style.display = msg ? 'block' : 'none';
  el.className = 'de-status ' + (cls || '');
  el.textContent = msg || '';
}

// Validate form, auto-generate ID if blank, build the device entry, write to config.json.
// On edit: replaces the existing entry by _deEditId; on add: checks for duplicate ID.
function saveDevEditor() {
  var id = (document.getElementById('de-id').value || '').trim();
  var type = document.getElementById('de-type').value;
  var isI2cType = I2C_SERVO_TYPES.indexOf(type) >= 0 || I2C_MOTOR_TYPES.indexOf(type) >= 0;
  var boardIdx = parseInt(document.getElementById('de-board').value, 10);
  // For I2C devices the board field is hidden — always use the context-locked index.
  if (isI2cType && _deFixedBoardApiIdx !== undefined) boardIdx = _deFixedBoardApiIdx;
  var addrStr = (document.getElementById('de-addr').value || '').trim();
  var defState = document.getElementById('de-defstate').value;
  var count = (_deviceTypes[type] || {}).wires !== undefined ? (_deviceTypes[type] || {}).wires : 1;

  if (!id) {
    var firstPinEl = document.querySelector('.de-w');
    var firstPin = (count > 0 && firstPinEl) ? (parseInt(firstPinEl.value, 10) || '') : '';
    var shortType = type.replace(/^MrJDB/, '').toLowerCase().replace(/[^a-z0-9]/g, '');
    var base = shortType + (firstPin !== '' ? firstPin : '');
    var existingIds = ((_dbgCfg && _dbgCfg.devices) || []).map(function (d) { return d.id; });
    id = base;
    var n = 2;
    while (existingIds.indexOf(id) >= 0 && id !== _deEditId) { id = base + '_' + n++; }
    document.getElementById('de-id').value = id;
  }

  var isServo = SERVO_TYPES.indexOf(type) >= 0;
  var isI2cServo2 = I2C_SERVO_TYPES.indexOf(type) >= 0;
  var isI2cMotor2 = I2C_MOTOR_TYPES.indexOf(type) >= 0;
  var wiring = [];
  if (count > 0) {
    var wInputs = document.querySelectorAll('.de-w');
    for (var i = 0; i < wInputs.length; i++) {
      var v = parseInt(wInputs[i].value, 10);
      var minW = isServo ? 1 : 0;
      if (isNaN(v) || v < minW || v > 253) {
        deStatus(isServo ? t('de.err_wiring_servo') : t('de.err_wiring'), 'err');
        return;
      }
      wiring.push(v);
    }
  }

  var board = _dbgBoards[boardIdx];
  // Fallback: runtime board list may be stale — try config boards (source of truth for id).
  if (!board && isI2cType && _dbgCfg && _dbgCfg.boards && _dbgCfg.boards[boardIdx])
    board = _dbgCfg.boards[boardIdx];
  if (!board) { deStatus(t('de.err_board'), 'err'); return; }

  var dev = { id: id, type: type, board: board.id };
  if (count === 1) dev.wiring = wiring[0];
  else if (count > 1) dev.wiring = wiring;
  if (addrStr) { var addr = parseInt(addrStr, 10); if (addr >= 1 && addr <= 10239) dev.address = addr; }
  if (defState) dev.default_state = defState;

  if (isI2cServo2) {
    var rows = document.querySelectorAll('#de-positions-list .de-pos-row');
    var positions = [];
    var angErr = false;
    rows.forEach(function (row) {
      var lbl = (row.querySelector('.de-pos-lbl').value || '').trim();
      var ang = parseInt(row.querySelector('.de-pos-ang').value, 10);
      var dur = parseInt(row.querySelector('.de-pos-dur').value, 10);
      var easeEl = row.querySelector('.de-pos-ease');
      var easeOut = easeEl ? easeEl.checked : false;
      if (isNaN(ang) || ang < -90 || ang > 90) { angErr = true; return; }
      if (isNaN(dur) || dur < 100) dur = 2000;
      var pos = { angle: ang, duration_ms: dur };
      if (lbl) pos.label = lbl;
      if (easeOut) pos.ease_out = true;
      positions.push(pos);
    });
    if (angErr) { deStatus('Angle invalide — plage : −90°..+90°', 'err'); document.getElementById('de-save-btn').disabled = false; return; }
    if (positions.length === 0) { deStatus(t('de.err_pos_empty'), 'err'); document.getElementById('de-save-btn').disabled = false; return; }
    dev.positions = positions;
    var pMinEl = document.getElementById('de-pulse-min');
    var pMaxEl = document.getElementById('de-pulse-max');
    var pMinV = pMinEl ? parseInt(pMinEl.value, 10) : 1000;
    var pMaxV = pMaxEl ? parseInt(pMaxEl.value, 10) : 2000;
    if (isNaN(pMinV) || pMinV < 500 || pMinV > 2500) pMinV = 1000;
    if (isNaN(pMaxV) || pMaxV < 500 || pMaxV > 2500) pMaxV = 2000;
    if (pMinV !== 1000 || pMaxV !== 2000) { dev.pulse_min_us = pMinV; dev.pulse_max_us = pMaxV; }
    else { delete dev.pulse_min_us; delete dev.pulse_max_us; }
  }
  if (isI2cMotor2) {
    var mRows = document.querySelectorAll('#de-motor-states-list .de-pos-row');
    var mStates = [];
    var mErr = false;
    mRows.forEach(function (row) {
      var lbl = (row.querySelector('.de-mst-lbl').value || '').trim();
      var spd = parseInt(row.querySelector('.de-mst-spd').value, 10);
      var dur = parseInt(row.querySelector('.de-mst-dur').value, 10);
      var rampUp = parseInt(row.querySelector('.de-mst-up').value, 10);
      var rampDn = parseInt(row.querySelector('.de-mst-dn').value, 10);
      if (isNaN(spd) || spd < -100 || spd > 100) { mErr = true; return; }
      if (isNaN(dur) || dur < 0) dur = 0;
      if (isNaN(rampUp) || rampUp < 0) rampUp = 0;
      if (isNaN(rampDn) || rampDn < 0) rampDn = 0;
      var ms = { speed: spd };
      if (dur > 0) ms.duration_ms = dur;
      if (rampUp > 0) ms.ramp_up_ms = rampUp;
      if (rampDn > 0) ms.ramp_down_ms = rampDn;
      if (lbl) ms.label = lbl;
      mStates.push(ms);
    });
    if (mErr) { deStatus(t('de.err_mst_speed'), 'err'); document.getElementById('de-save-btn').disabled = false; return; }
    if (mStates.length === 0) { deStatus(t('de.err_mst_empty'), 'err'); document.getElementById('de-save-btn').disabled = false; return; }
    dev.states = mStates;
    delete dev.speed; // remove legacy single-speed field
    var neuEl = document.getElementById('de-motor-neutral');
    var neuV = neuEl ? parseInt(neuEl.value, 10) : 1500;
    if (isNaN(neuV) || neuV < 1000 || neuV > 2000) {
      deStatus(t('de.err_mst_neutral'), 'err');
      document.getElementById('de-save-btn').disabled = false;
      return;
    }
    if (neuV !== 1500) dev.neutral_us = neuV;
    else delete dev.neutral_us;
  }

  document.getElementById('de-save-btn').disabled = true;
  deStatus(t('de.saving'), 'ok');

  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      if (_deEditId) {
        var replaced = false;
        for (var i = 0; i < cfg.devices.length; i++) {
          if (cfg.devices[i].id === _deEditId) { cfg.devices[i] = dev; replaced = true; break; }
        }
        if (!replaced) cfg.devices.push(dev);
      } else {
        for (var j = 0; j < cfg.devices.length; j++) {
          if (cfg.devices[j].id === id) {
            deStatus(t('de.err_dup'), 'err');
            document.getElementById('de-save-btn').disabled = false;
            return Promise.reject(null);
          }
        }
        cfg.devices.push(dev);
      }
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () {
      _reloadAfterSave();
      closeDevEditor();
      loadDebug();
    })
    .catch(function (e) {
      if (e) { deStatus(t('de.err_prefix') + e.message, 'err'); document.getElementById('de-save-btn').disabled = false; }
    });
}

// Delete the currently edited device from config.json after confirmation.
function deleteDevEditor() {
  if (!_deEditId || !confirm(t('de.del_confirm', { id: _deEditId }))) return;
  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      cfg.devices = cfg.devices.filter(function (d) { return d.id !== _deEditId; });
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () {
      _reloadAfterSave();
      closeDevEditor();
      loadDebug();
    })
    .catch(function (e) { deStatus(t('de.err_prefix') + e.message, 'err'); });
}

/* ── Board editor ───────────────────────────────────────────────────── */

var _beEditIdx = -1; // index in _dbgCfg.boards[] being edited; -1 means new board

// Map PlatformIO env names (lowercased, stripped of _ - spaces) to board type keys in board_types.json.
// Used to pre-select the right board type on first run and in resetConfig().
var _ENV_TO_BOARD = {
  'esp32devkitc': 'ESP32DevkitC',
  'esp32devkitcbreadboard': 'ESP32DevkitC',
  'esp32minibreadboard': 'ESP32Mini',
  'esp32mini': 'ESP32Mini',
  'mrjfxv1': 'MrJRailwayFX_v1'
};

// Return the board type key that best matches the current firmware env,
// falling back to the first available type if the env is unknown.
function guessDefaultBoardType() {
  var env = (_dbgStatus && _dbgStatus.env || '').toLowerCase().replace(/[_\s-]/g, '');
  var matched = _ENV_TO_BOARD[env];
  if (matched && _boardTypes[matched]) return matched;
  return Object.keys(_boardTypes)[0] || 'ESP32DevkitC';
}

// Open the board add/edit modal.  cfgIdx is the index in _dbgCfg.boards[], or null for new.
function openBoardEditor(cfgIdx) {
  _beEditIdx = cfgIdx !== null ? cfgIdx : -1;
  var board = (cfgIdx !== null && cfgIdx >= 0 && _dbgCfg) ? _dbgCfg.boards[cfgIdx] : null;

  // Populate type select
  var typeEl = document.getElementById('be-type');
  var buses = (_dbgCfg && _dbgCfg.buses) || {};
  var feats = (_dbgStatus && _dbgStatus.features) || {};
  var isNew = (_beEditIdx < 0);
  // Check if a main GPIO board (busType===null) already exists
  var hasMainBoard = isNew && ((_dbgCfg && _dbgCfg.boards) || []).some(function (b) {
    return !(_boardTypes[b.type] && _boardTypes[b.type].busType);
  });
  // Build the board-type <select>.  Rules for disabling an option:
  //   – Main board (busType===null) disabled if another main board already exists.
  //   – Expansion board disabled if no compatible bus is configured and no firmware
  //     feature flag covers its bus type (feature flags allow pre-configuration before
  //     the bus entry is actually created).
  typeEl.innerHTML = Object.keys(_boardTypes).map(function (k) {
    var def = _boardTypes[k];
    var required = def.busType || null;
    var isMainType = !required;
    // Feature flag unlocks bus-type boards even before a bus is configured
    var featureCovers = (required === 'spi_master_only' && feats.spi)
      || (required === 'i2c' && (feats.oled || feats.i2c))
      || (required === 'uart' && (feats.lobot_servo || feats.lx16a_servo || feats.audio));
    var hasCompatBus = !required || featureCovers || Object.keys(buses).some(function (bk) {
      return buses[bk].type === required;
    });
    var conflict = isMainType && hasMainBoard;
    var disabled = conflict || !hasCompatBus;
    var tip = conflict ? t('be.one_gpio_board_only') : t('be.no_bus_warn');
    var dis = disabled ? ' disabled title="' + tip.replace(/"/g, '&quot;') + '"' : '';
    var label = (def.label || k) + (disabled ? ' \u26a0' : '');
    return '<option value="' + k + '"' + dis + '>' + label + '</option>';
  }).join('');

  if (board) {
    document.getElementById('be-title').textContent = t('be.edit_prefix') + board.id;
    document.getElementById('be-id').value = board.id;
    document.getElementById('be-id').disabled = true;
    typeEl.value = board.type || '';
    document.getElementById('be-del-btn').style.display = '';
  } else {
    document.getElementById('be-title').textContent = t('be.new');
    document.getElementById('be-id').value = '';
    document.getElementById('be-id').disabled = false;
    typeEl.value = guessDefaultBoardType();
    document.getElementById('be-del-btn').style.display = 'none';
  }

  beUpdateFields(board);
  beStatus('', '');
  document.getElementById('be-save-btn').disabled = false;
  document.getElementById('be-overlay').style.display = 'block';
  document.getElementById('be-modal').style.display = 'flex';
  applyLang();
}

// Update the board ID placeholder from the selected type name (only when the field is empty).
function beUpdateIdPlaceholder() {
  var idEl = document.getElementById('be-id');
  if (!idEl || idEl.disabled || idEl.value.trim()) return;
  var type = document.getElementById('be-type').value;
  var base = type.toLowerCase().replace(/[^a-z0-9]/g, '');
  idEl.placeholder = base || 'auto';
}

// Rebuild the bus selector and pin_count field based on the selected board type.
// MCU boards have no bus; SPI boards show a pin_count field; others show a bus dropdown.
function beUpdateFields(board) {
  var type = document.getElementById('be-type').value;
  var def = _boardTypes[type] || {};
  var requiredBusType = def.busType || null;

  beUpdateIdPlaceholder();

  // Bus select
  var busEl = document.getElementById('be-bus');
  var buses = (_dbgCfg && _dbgCfg.buses) || {};
  var compatKeys = requiredBusType
    ? Object.keys(buses).filter(function (k) { return buses[k].type === requiredBusType; })
    : [];

  if (!requiredBusType) {
    // MCU board — no bus required: show disabled select with single "none" option
    busEl.innerHTML = '<option value="">' + t('be.bus_none') + '</option>';
    busEl.disabled = true;
    beStatus('', '');
  } else if (compatKeys.length === 0) {
    // Bus required but none configured yet
    busEl.innerHTML = '<option value="">' + t('be.bus_missing').replace('{{type}}', requiredBusType) + '</option>';
    busEl.disabled = true;
    beStatus(t('be.no_bus_warn').replace('{{type}}', requiredBusType), 'warn');
  } else {
    // Compatible buses exist — show only them (bus is required, so no "none" option)
    busEl.innerHTML = compatKeys.map(function (k) {
      return '<option value="' + k + '">' + k + ' (' + (buses[k].type || '') + ')</option>';
    }).join('');
    busEl.disabled = false;
    // Restore selection when editing; otherwise auto-select first
    if (board && board.bus && compatKeys.indexOf(board.bus) >= 0) {
      busEl.value = board.bus;
    } else {
      busEl.value = compatKeys[0];
    }
    beStatus('', '');
  }

  // pin_count: only for SPI boards
  var isSpi = requiredBusType === 'spi_master_only';
  var pcField = document.getElementById('be-pincount-field');
  pcField.style.display = isSpi ? '' : 'none';
  if (isSpi && board && board.pin_count) {
    document.getElementById('be-pincount').value = board.pin_count;
  } else if (!board || !isSpi) {
    document.getElementById('be-pincount').value = '';
  }

  // i2c_address + oscillator_hz: only for I2C boards
  var isI2c = requiredBusType === 'i2c';
  var i2cField = document.getElementById('be-i2c-field');
  var oscField = document.getElementById('be-osc-field');
  var oscHint = document.getElementById('be-osc-hint');
  if (i2cField) i2cField.style.display = isI2c ? '' : 'none';
  if (oscField) oscField.style.display = isI2c ? '' : 'none';
  if (oscHint) oscHint.textContent = t('be.osc_hint');
  if (isI2c) {
    document.getElementById('be-i2c-addr').value = (board && board.i2c_address !== undefined) ? board.i2c_address : 64;
    document.getElementById('be-osc-hz').value = (board && board.oscillator_hz) ? board.oscillator_hz : '';
  }
}

// Close the board editor modal without saving.
function closeBoardEditor() {
  document.getElementById('be-overlay').style.display = 'none';
  document.getElementById('be-modal').style.display = 'none';
}

// Show or hide the board editor inline status message (cls: 'ok' | 'err' | 'warn').
function beStatus(msg, cls) {
  var el = document.getElementById('be-status');
  el.style.display = msg ? 'block' : 'none';
  el.className = 'de-status ' + (cls || '');
  el.textContent = msg || '';
}

// Validate form, auto-generate ID if blank, write the board entry to config.json.
// On edit: replaces by _beEditIdx; on add: checks for duplicate ID.
function saveBoardEditor() {
  var id = document.getElementById('be-id').disabled
    ? document.getElementById('be-id').value
    : (document.getElementById('be-id').value || '').trim();
  var type = document.getElementById('be-type').value;
  var bus = document.getElementById('be-bus').value;
  var pc = parseInt(document.getElementById('be-pincount').value, 10);

  if (!id) {
    var base = type.toLowerCase().replace(/[^a-z0-9]/g, '') || 'board';
    var existingIds = ((_dbgCfg && _dbgCfg.boards) || []).map(function (b) { return b.id; });
    id = base;
    var n = 2;
    while (existingIds.indexOf(id) >= 0) { id = base + '_' + n++; }
    document.getElementById('be-id').value = id;
  }
  if (!type) { beStatus(t('be.err_type'), 'err'); return; }
  var def = _boardTypes[type] || {};
  if (def.busType && !bus) { beStatus(t('be.err_bus_required').replace('{{type}}', def.busType), 'err'); return; }

  var entry = { id: id, type: type };
  if (bus) entry.bus = bus;
  if (def.busType === 'spi_master_only' && pc > 0) entry.pin_count = pc;
  if (def.busType === 'i2c') {
    var i2cAddr = parseInt(document.getElementById('be-i2c-addr').value, 10);
    if (!isNaN(i2cAddr) && i2cAddr !== 64) entry.i2c_address = i2cAddr;
    var oscHz = parseInt(document.getElementById('be-osc-hz').value, 10);
    if (!isNaN(oscHz) && oscHz !== 25000000) entry.oscillator_hz = oscHz;
  }

  document.getElementById('be-save-btn').disabled = true;
  beStatus(t('de.saving'), 'ok');

  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      if (!cfg.boards) cfg.boards = [];
      if (_beEditIdx >= 0) {
        cfg.boards[_beEditIdx] = entry;
      } else {
        for (var i = 0; i < cfg.boards.length; i++) {
          if (cfg.boards[i].id === id) {
            beStatus(t('be.err_dup'), 'err');
            document.getElementById('be-save-btn').disabled = false;
            return Promise.reject(null);
          }
        }
        cfg.boards.push(entry);
      }
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { _reloadAfterSave(); closeBoardEditor(); loadDebug(); })
    .catch(function (e) {
      if (e) { beStatus(t('de.err_prefix') + e.message, 'err'); document.getElementById('be-save-btn').disabled = false; }
    });
}

// Delete a board from config.json; also removes all devices that were assigned to that board.
function deleteBoard(id) {
  if (!confirm(t('be.del_confirm').replace('{{id}}', id))) return;
  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      cfg.boards = cfg.boards.filter(function (b) { return b.id !== id; });
      cfg.devices = cfg.devices.filter(function (d) { return d.board !== id; });
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { _reloadAfterSave(); loadDebug(); })
    .catch(function (e) { alert(t('de.err_prefix') + e.message); });
}

// Close the board editor then delete the board (called from the Delete button inside the modal).
function deleteBoardEditor() {
  closeBoardEditor();
  deleteBoard(document.getElementById('be-id').value);
}

/* ── Bus editor ──────────────────────────────────────────────────────── */

var _bueEditKey = null; // bus key currently being edited; null means new bus

// Add a pre-wired "linked bus" suggestion to cfg.buses (idempotent — skipped if already present).
// Linked buses are defined in board_types.json linkedBuses[] with hard-coded pin values
// matching the board's PCB traces.
function addLinkedBus(btType, lbKey) {
  var def = _boardTypes[btType] || {};
  var lb = (def.linkedBuses || []).filter(function (b) { return b.key === lbKey; })[0];
  if (!lb) return;
  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      if (!cfg.buses) cfg.buses = {};
      if (cfg.buses[lb.key]) return Promise.resolve(null); // already present
      var entry = { type: lb.type };
      ((_busTypes[lb.type] || {}).fields || []).forEach(function (f) {
        if (lb[f.key] !== undefined) entry[f.key] = lb[f.key];
      });
      cfg.buses[lb.key] = entry;
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (r && !r.ok) throw new Error('HTTP ' + r.status); _reloadAfterSave(); loadDebug(); })
    .catch(function (e) { alert(t('de.err_prefix') + e.message); });
}

// Render the buses tab, which shows three layers:
//   1. Structural buses — read-only, deduced from /api/status sys_pins + features
//      (uart0 debug, DCC).  These are compile-time and cannot be edited here.
//   2. Applicative buses — editable, from cfg.buses (spi, uart, i2c).
//   3. Linked bus suggestions — pre-wired buses defined in board_types.json
//      for boards currently in the config but not yet in cfg.buses.
function renderBusesTab() {
  var el = document.getElementById('buses-list');
  if (!el) return;
  var cfg = _dbgCfg;
  if (!cfg) { el.innerHTML = '<div class="prm-info">' + t('prm.loading') + '</div>'; return; }

  var html = '';

  // ── Bus structurels (lecture seule, déduits de sys_pins + features) ──
  var sp = (_dbgStatus && _dbgStatus.sys_pins) || {};
  var feat = (_dbgStatus && _dbgStatus.features) || {};

  // Render the title bar of a bus card with a type badge.
  function busTitle(key, type) {
    return '<div class="bus-card-title">'
      + key
      + '<span class="bus-type-badge">' + type + '</span>'
      + '</div>';
  }
  // Render one label/value row inside a bus card; shows em-dash when value is absent.
  function busRow(lbl, val) {
    return '<div class="bus-row"><span class="bus-lbl">' + lbl + '</span><span class="bus-val">' + (val !== undefined && val !== null ? val : '—') + '</span></div>';
  }

  if (sp['1'] === 'TX0' || sp['3'] === 'RX0') {
    html += '<div class="bus-card bus-structural">'
      + busTitle('uart0 (debug)', 'uart')
      + busRow('TX', 1)
      + busRow('RX', 3)
      + '</div>';
  }

  if (feat.dcc) {
    var dccPin = null;
    Object.keys(sp).forEach(function (g) { if (sp[g] === 'DCC') dccPin = g; });
    html += '<div class="bus-card bus-structural">'
      + busTitle('dcc', 'dcc')
      + busRow('PIN', dccPin)
      + '</div>';
  }

  // ── Bus applicatifs (config.json) ──
  var buses = cfg.buses || {};
  var keys = Object.keys(buses);

  if (html === '' && keys.length === 0) {
    el.innerHTML = '<div class="prm-info">' + t('bue.no_buses') + '</div>';
    return;
  }

  html += keys.map(function (k) {
    var bus = buses[k];
    var fields = (_busTypes[bus.type] || {}).fields || [];
    var rows = fields.map(function (f) {
      return busRow(f.label.split(' ')[0], bus[f.key]);
    }).join('');
    var ks = k.replace(/'/g, "\\'");
    return '<div class="bus-card">'
      + busTitle(k, bus.type || '?')
      + rows
      + '<div class="bus-card-actions">'
      + '<button class="dbg-hbtn" onclick="openBusEditor(\'' + ks + '\')">' + t('bue.edit') + '</button>'
      + '<button class="dbg-hbtn off" onclick="deleteBus(\'' + ks + '\')">' + t('de.del') + '</button>'
      + '</div>'
      + '</div>';
  }).join('');

  // ── Linked bus suggestions (from board types, not yet in cfg.buses) ──
  var seenLbKeys = {};
  ((_dbgCfg && _dbgCfg.boards) || []).forEach(function (b) {
    var def = _boardTypes[b.type] || {};
    (def.linkedBuses || []).forEach(function (lb) {
      if (buses[lb.key] || seenLbKeys[lb.key]) return;
      seenLbKeys[lb.key] = true;
      var fields = (_busTypes[lb.type] || {}).fields || [];
      var rows = fields.map(function (f) {
        return busRow(f.label.split(' ')[0], lb[f.key]);
      }).join('');
      var lks = lb.key.replace(/'/g, "\\'");
      var bts = b.type.replace(/'/g, "\\'");
      html += '<div class="bus-card bus-suggestion">'
        + busTitle(lb.label || lb.key, lb.type || '?')
        + rows
        + '<div class="bus-card-actions">'
        + '<button class="dbg-hbtn" onclick="addLinkedBus(\'' + bts + '\',\'' + lks + '\')">' + t('bue.add') + '</button>'
        + '</div>'
        + '</div>';
    });
  });

  el.innerHTML = html;
}

// Open the bus editor modal, pre-populated with existing bus data when key is given (edit mode) or blank (add mode).
function openBusEditor(key) {
  _bueEditKey = key || null;
  var busData = (key && _dbgCfg && _dbgCfg.buses && _dbgCfg.buses[key]) || null;
  var feat = (_dbgStatus && _dbgStatus.features) || {};

  // Available bus types filtered by compiled-in firmware features.
  var availTypes = ['i2c', 'uart', 'dcc'];
  if (feat.spi) availTypes.push('spi_master_only');

  var typeEl = document.getElementById('bue-type');
  typeEl.innerHTML = availTypes.map(function (tp) {
    return '<option value="' + tp + '">' + tp + '</option>';
  }).join('');

  if (key && busData) {
    document.getElementById('bue-title').textContent = t('bue.edit_prefix') + key;
    document.getElementById('bue-key').value = key;
    document.getElementById('bue-key').disabled = true;
    typeEl.value = busData.type || availTypes[0];
    document.getElementById('bue-del-btn').style.display = '';
  } else {
    document.getElementById('bue-title').textContent = t('bue.new');
    document.getElementById('bue-key').disabled = false;
    typeEl.value = availTypes[0];
    busData = null;
    document.getElementById('bue-del-btn').style.display = 'none';
    // Suggest a unique key derived from the selected type.
    var existingBuses = (_dbgCfg && _dbgCfg.buses) || {};
    var base = availTypes[0].replace('_master_only', '').replace(/_.*/, '');
    var suggested = base;
    var n = 2;
    while (existingBuses[suggested]) { suggested = base + n++; }
    document.getElementById('bue-key').value = suggested;
  }

  bueUpdateFields(busData);

  bueStatus('', '');
  document.getElementById('bue-save-btn').disabled = false;
  document.getElementById('bue-overlay').style.display = 'block';
  document.getElementById('bue-modal').style.display = 'flex';
  applyLang();
}

// Rebuild the bus field inputs for the selected bus type.
// GPIO fields are pre-filled with the placeholder value unless that GPIO is already taken
// by another bus or by sys_pins; baud gets the placeholder unconditionally.
function bueUpdateFields(busData) {
  var type = document.getElementById('bue-type').value;
  var fields = (_busTypes[type] || {}).fields || [];

  // In add mode, refresh the key suggestion when the type changes,
  // but only if the key still looks auto-generated (matches a known type base).
  if (!_bueEditKey) {
    var keyEl = document.getElementById('bue-key');
    var existingBuses = (_dbgCfg && _dbgCfg.buses) || {};
    var base = type.replace('_master_only', '').replace(/_.*/, '');
    var suggested = base;
    var n = 2;
    while (existingBuses[suggested]) { suggested = base + n++; }
    var cur = keyEl.value;
    var looksAuto = !cur || /^[a-z]+\d*$/.test(cur);
    if (looksAuto) keyEl.value = suggested;
  }

  // Build set of GPIO pins already in use (sys_pins + existing buses, excluding the one being edited)
  var usedGpios = {};
  Object.keys((_dbgStatus && _dbgStatus.sys_pins) || {}).forEach(function (g) {
    usedGpios[parseInt(g)] = true;
  });
  Object.keys((_dbgCfg && _dbgCfg.buses) || {}).forEach(function (k) {
    if (k === _bueEditKey) return; // skip bus being edited
    var b = (_dbgCfg.buses)[k];
    ((_busTypes[b.type] || {}).fields || []).forEach(function (f) {
      if (f.key !== 'baud' && b[f.key] !== undefined) usedGpios[parseInt(b[f.key])] = true;
    });
  });

  document.getElementById('bue-fields').innerHTML = fields.map(function (f) {
    var val;
    if (busData && busData[f.key] !== undefined) {
      val = busData[f.key];
    } else if (f.key === 'baud') {
      val = f.placeholder;
    } else {
      var suggested = parseInt(f.placeholder);
      val = (!isNaN(suggested) && !usedGpios[suggested]) ? suggested : '';
    }
    return '<div class="de-field">'
      + '<label>' + f.label + '</label>'
      + '<input type="number" id="bue-' + f.key + '" min="' + f.min + '" max="' + f.max + '"'
      + ' placeholder="' + f.placeholder + '" value="' + val + '">'
      + '</div>';
  }).join('');
}

// Close the bus editor modal without saving.
function closeBusEditor() {
  document.getElementById('bue-overlay').style.display = 'none';
  document.getElementById('bue-modal').style.display = 'none';
}

// Show or hide the bus editor inline status message (cls: 'ok' | 'err' | 'warn').
function bueStatus(msg, cls) {
  var el = document.getElementById('bue-status');
  el.style.display = msg ? 'block' : 'none';
  el.className = 'de-status ' + (cls || '');
  el.textContent = msg || '';
}

// Validate all bus fields (range check per field descriptor), write the bus entry to config.json.
// Each field is validated in order; the loop sets valid=false and returns early on first error
// so the forEach continues iterating but skips remaining fields (can't break out of forEach).
function saveBusEditor() {
  var key = _bueEditKey || (document.getElementById('bue-key').value || '').trim();
  if (!key) { bueStatus(t('bue.err_key'), 'err'); return; }

  var type = document.getElementById('bue-type').value;
  var bus = { type: type };
  var fields = (_busTypes[type] || {}).fields || [];
  var valid = true;
  fields.forEach(function (f) {
    if (!valid) return;
    var el = document.getElementById('bue-' + f.key);
    var n = el ? parseInt(el.value, 10) : NaN;
    if (isNaN(n) || n < f.min || n > f.max) {
      bueStatus(t('bue.err_field') + f.label, 'err');
      valid = false;
      return;
    }
    bus[f.key] = n;
  });
  if (!valid) return;

  document.getElementById('bue-save-btn').disabled = true;
  bueStatus(t('de.saving'), 'ok');

  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      if (!cfg.buses) cfg.buses = {};
      if (!_bueEditKey && cfg.buses[key]) {
        bueStatus(t('bue.err_dup'), 'err');
        document.getElementById('bue-save-btn').disabled = false;
        return Promise.reject(null);
      }
      cfg.buses[key] = bus;
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { _reloadAfterSave(); closeBusEditor(); loadDebug(); })
    .catch(function (e) {
      if (e) { bueStatus(t('de.err_prefix') + e.message, 'err'); document.getElementById('bue-save-btn').disabled = false; }
    });
}

// Delete a bus from config.json; also removes all boards that depended on it.
function deleteBus(key) {
  if (!confirm(t('bue.del_confirm').replace('{{key}}', key))) return;
  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      delete cfg.buses[key];
      // Remove boards that depended on this bus
      cfg.boards = cfg.boards.filter(function (b) { return b.bus !== key; });
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function () { _reloadAfterSave(); loadDebug(); renderBusesTab(); })
    .catch(function (e) { alert(t('de.err_prefix') + e.message); });
}

// Close the bus editor then delete the bus (called from the Delete button inside the modal).
function deleteBusEditor() {
  var key = _bueEditKey;
  closeBusEditor();
  deleteBus(key);
}

/* ── Boot ───────────────────────────────────────────────────────────── */
// Initialisation sequence (order matters — theme before applyLang, lang before poll):
//   1. Restore user prefs (poll interval, theme)
//   2. Render dirty banner (localStorage state from previous session)
//   3. Apply language / i18n
//   4. Route to the view specified in the URL hash
//   5. Start the first poll (triggers showWelcome() if device list is empty)
(function () {
  var stored = parseInt(localStorage.getItem('poll') || '0', 10);
  if (stored >= 500) POLL = stored;
  document.getElementById('prm-poll').value = POLL;
})();
var _th = localStorage.getItem('mrj-theme') || 'night';
if (_th !== 'night') document.body.classList.add('th-' + _th);
renderDirtyBanner();
document.querySelectorAll('.theme-dot').forEach(function (b) {
  b.classList.toggle('active', b.classList.contains('theme-dot-' + _th));
});
applyLang();
(function () {
  var h = location.hash.slice(1);
  var valid = ['cockpit', 'canvas', 'params', 'config', 'about']; // 'params' redirects to 'about'
  if (h && valid.indexOf(h) >= 0) switchView(h);
})();
window.addEventListener('hashchange', function () {
  var h = location.hash.slice(1);
  if (h && h !== _currentView && document.getElementById('view-' + h)) switchView(h);
});
// Fetch status early so _dbgStatus is available for view-specific rendering (e.g. files tab buttons).
fetch('/api/status')
  .then(function (r) { return r.json(); })
  .then(function (st) {
    if (st && st.env) _dbgStatus = st;
    if (_currentCfgTab === 'files' && _currentView === 'config') loadConfigs();
  })
  .catch(function () { });
// Load device-type metadata before the first poll so cockpit cards render correctly.
fetch('/api/device-types')
  .then(function (r) { return r.json(); })
  .then(function (dt) { _applyDeviceTypes(dt); poll(); })
  .catch(function () { poll(); });
_pollTimer = setInterval(poll, POLL);

