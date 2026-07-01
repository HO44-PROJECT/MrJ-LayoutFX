/**
 * @file app-board-editor.js
 * @brief Board editor modal for managing I2C/SPI expansion boards.
 *
 * Handles creation and editing of expansion boards (PCA9685, MCP23017, 74HC595)
 * in the WebUI configuration panel.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

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
    var label = (def.label || k) + (disabled ? ' ⚠' : '');
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

// (Re)enable the uart0 serial-log bus: keeps Tier-2 logging on and reserves GPIO1/3.
// Removing it (deleteBus('uart0')) frees those pins for use as effect outputs.
function addLogBus() {
  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      if (!cfg.buses) cfg.buses = {};
      if (cfg.buses.uart0) return Promise.resolve(null); // already present
      cfg.buses.uart0 = { type: 'uart', tx: 1, rx: 3, baud: 115200 };
      return fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cfg)
      });
    })
    .then(function (r) { if (r && !r.ok) throw new Error('HTTP ' + r.status); _reloadAfterSave(); loadDebug(); })
    .catch(function (e) { alert(t('de.err_prefix') + e.message); });
}

// Add the DCC bus → arms the NmraDcc decoder on its pin (compile-time DCC_PIN,
// read from sys_pins). Removing it (deleteBus('dcc')) disables DCC. Like the log bus.
function addDccBus(pin) {
  if (pin === null || pin === undefined) { alert(t('de.err_prefix') + 'DCC pin?'); return; }
  fetch('/api/config')
    .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function (cfg) {
      if (!cfg.buses) cfg.buses = {};
      if (cfg.buses.dcc) return Promise.resolve(null); // already present
      cfg.buses.dcc = { type: 'dcc', pin: parseInt(pin, 10) };
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

  // ── Bus log UART0 (actionnable) ──
  // Présent dans cfg.buses → rendu plus bas (carte éditable + bouton Supprimer).
  // Absent mais supporté (log/debug série compilé) → suggestion pour le (ré)activer en 1 clic.
  if ((feat.log_serial || feat.debug_serial) && !(cfg.buses && cfg.buses.uart0)) {
    html += '<div class="bus-card bus-suggestion">'
      + busTitle('uart0 (log)', 'uart')
      + busRow('TX', 1)
      + busRow('RX', 3)
      + '<div style="font-size:.7rem;color:var(--t2);margin:.3rem 0">' + t('bue.log_hint') + '</div>'
      + '<div class="bus-card-actions">'
      + '<button class="dbg-hbtn" onclick="addLogBus()">' + t('bue.add') + '</button>'
      + '</div>'
      + '</div>';
  }

  // ── Bus DCC (actionnable, comme uart0) ──
  // Présent dans cfg.buses → rendu plus bas (éditable + Supprimer).
  // Absent mais compilé (feat.dcc) → suggestion "+ Ajouter" pour armer le décodeur.
  if (feat.dcc && !(cfg.buses && cfg.buses.dcc)) {
    var dccPin = null;
    Object.keys(sp).forEach(function (g) { if (sp[g] === 'DCC') dccPin = g; });
    html += '<div class="bus-card bus-suggestion">'
      + busTitle('dcc', 'dcc')
      + busRow('PIN', dccPin)
      + '<div style="font-size:.7rem;color:var(--t2);margin:.3rem 0">' + t('bue.dcc_hint') + '</div>'
      + '<div class="bus-card-actions">'
      + '<button class="dbg-hbtn" onclick="addDccBus(' + (dccPin !== null ? dccPin : 'null') + ')">' + t('bue.add') + '</button>'
      + '</div>'
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
    // uart0 is the serial-log console: fixed pins (1/3), no Edit — only Remove
    // (which frees GPIO1/3). Other buses keep Edit + Remove.
    var isLog = (k === 'uart0');
    var fields = (_busTypes[bus.type] || {}).fields || [];
    var rows = fields.map(function (f) {
      return busRow(f.label.split(' ')[0], bus[f.key]);
    }).join('');
    var ks = k.replace(/'/g, "\\'");
    return '<div class="bus-card">'
      + busTitle(isLog ? 'uart0 (log)' : k, bus.type || '?')
      + rows
      + (isLog ? '<div style="font-size:.7rem;color:var(--t2);margin:.3rem 0">' + t('bue.log_active_hint') + '</div>' : '')
      + '<div class="bus-card-actions">'
      + (isLog ? '' : '<button class="dbg-hbtn" onclick="openBusEditor(\'' + ks + '\')">' + t('bue.edit') + '</button>')
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
document.querySelectorAll('.pinlbl-btn').forEach(function (b) {
  b.classList.toggle('active', b.getAttribute('data-mode') === _dbgPinLabel);
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
