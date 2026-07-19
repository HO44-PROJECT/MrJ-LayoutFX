/**
 * @file app-boards.js
 * @brief I2C scanner and board management for the configuration view.
 *
 * Handles I2C device detection, board listing, and bus configuration in the
 * WebUI config panel.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

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
      var html = '<span class="i2c-pins">SDA GPIO' + sda + ' / SCL GPIO' + scl + '</span> ';
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
    // Expose the FULL wiring (not just the matched pin) so multi-pin devices can
    // compute their anchor/linked pins in the board view.
    if (match) return { id: cd.id, type: cd.type, desired: -1, addr: cd.address, pins: Array.isArray(w) ? w.slice() : [w], _cfgOnly: true };
  }
  return null;
}

// Find a device's raw config entry (board+wiring match), regardless of runtime
// state — needed for fields like start_delay_ms that /api/devices doesn't carry.
function dbgFindCfgDev(boardApiIdx, wiring) {
  var boardId = _dbgCfg && _dbgCfg.boards && _dbgCfg.boards[boardApiIdx]
    ? _dbgCfg.boards[boardApiIdx].id : null;
  if (!boardId) return null;
  var cfgDevs = (_dbgCfg && _dbgCfg.devices) || [];
  for (var i = 0; i < cfgDevs.length; i++) {
    var cd = cfgDevs[i];
    if (cd.board !== boardId) continue;
    var w = cd.wiring;
    var match = Array.isArray(w) ? w.indexOf(wiring) >= 0 : w === wiring;
    if (match) return cd;
  }
  return null;
}

// Format a device's configured startup delay (#8) as a compact pin label,
// e.g. "4.0s+2s" (fixed + random upper bound), "500ms" (fixed only, <1s), or
// null when neither start_delay_ms nor start_delay_random_ms is set.
function fmtStartDelayLabel(cfgDev) {
  if (!cfgDev) return null;
  var fixed = cfgDev.start_delay_ms | 0;
  var rand = cfgDev.start_delay_random_ms | 0;
  if (fixed <= 0 && rand <= 0) return null;
  function fmt(ms) { return ms >= 1000 ? (ms / 1000) + 's' : ms + 'ms'; }
  var label = fmt(fixed);
  if (rand > 0) label += '+' + fmt(rand);
  return label;
}

// ── Rendering ────────────────────────────────────────────────────────

// Captured once at load, before applyLayoutName ever touches document.title —
// %%BRAND%% is already substituted server-side by then (#67).
var _dbgBrandTitle = document.title;

// Sync the layout name to both the header subtitle, the settings input field, and
// the browser tab title (#67 — distinguishes devices/layouts across open tabs).
// Does not update the input if it currently has focus (prevents overwriting user typing).
function applyLayoutName(name) {
  var span = document.getElementById('hdr-layout-name');
  if (span) span.textContent = name ? ' — ' + name : '';
  var inp = document.getElementById('cfg-layout-name');
  if (inp && inp !== document.activeElement) inp.value = name || '';
  document.title = name ? _dbgBrandTitle + ' — ' + name : _dbgBrandTitle;
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
// idle_pins is recomputed from cfg before every save so it always reflects the
// current device/bus assignments — no manual button needed.
function saveCfg(cfg) {
  cfg.idle_pins = computeIdlePins(cfg);
  fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(cfg) })
    .then(function (r) { if (r.ok) { clearDirty(); _reloadAfterSave(); } })
    .catch(function () { });
}

// Structural output count of a board type = pins carrying a "wiring" entry in
// board_types.json — the same rule the firmware's embedded pin-count table uses.
function _btStructuralPins(type) {
  var def = _boardTypes[type];
  if (!def || !def.pins) return 0;
  var n = 0;
  for (var i = 0; i < def.pins.length; i++) {
    if (def.pins[i].wiring !== undefined) n++;
  }
  return n;
}

// Renders the boards tab DIP diagram list.
// _dbgCfg.boards is the source of truth so newly saved boards appear without reboot.
// Runtime data (spiRank, pinCount) from _dbgBoards is overlaid when available.
function renderDebugBoards() {
  _grpInitDelegation(); // wire up linked-pins hover/click highlighting (once)
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
      // Runtime value first; otherwise the structural count of the type
      // (pin_count is no longer written by the editor; legacy configs may
      // still carry one — used as last resort only).
      pinCount: rt ? rt.pinCount : (_btStructuralPins(cb.type) || cb.pin_count || 0),
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
    ? ' <span class="dbg-idx-badge">board ' + board.spiRank + '</span>'
    : ' <span class="dbg-idx-badge">GPIO</span>';
  var name = tbt(board.type, 'label', (def && def.label) ? def.label : board.type);
  // Card header shows the SHORT product name (part before the em-dash): the full
  // label is already repeated on the chip strip at the centre of the drawing.
  // Boards without a PCB drawing (rows=0 bus boards) keep the full label.
  if (def && def.rows) name = name.split('—')[0].trim();

  var cfgIdx = board._cfgIdx !== undefined ? board._cfgIdx : -1;
  return '<div class="dbg-board">'
    + '<div class="dbg-board-hdr">'
    + '<span class="dbg-board-name">' + name + badge + '</span>'
    + '<div class="dbg-board-actions">'
    + '<button class="dbg-hbtn on" title="' + t('dbg.all_on_tip') + '" onclick="dbgAll(' + boardApiIdx + ',1)">' + t('dbg.all_on') + '</button>'
    + '<button class="dbg-hbtn off" title="' + t('dbg.all_off_tip') + '" onclick="dbgAll(' + boardApiIdx + ',0)">' + t('dbg.all_off') + '</button>'
    + (cfgIdx >= 0 ? '<button class="dbg-hbtn" title="' + t('be.edit_tip') + '" onclick="openBoardEditor(' + cfgIdx + ')">' + t('be.edit') + '</button>' : '')
    + (cfgIdx >= 0 ? '<button class="dbg-hbtn off" title="' + t('be.del_tip') + '" onclick="deleteBoard(\'' + board.id.replace(/'/g, "\\'") + '\')">' + t('de.del') + '</button>' : '')
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
      if (cfgDev.comment !== undefined) merged.comment = cfgDev.comment;
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
  // Connector entries WITH a pins list render as informative pin blocks
  // (renderBusConn, e.g. I2C/Serial on the LayoutFX v1 mother board); entries
  // WITHOUT pins render as small edge-connector badges of the same nature as
  // the USB symbol, flanking the chip strip (vertically centred) — e.g. the
  // SPI IN / SPI OUT headers of the LayoutFX SPI Child daughter card.
  var _allConns = def.connectors || [];
  var _lSym = _allConns.filter(function (c) { return c.side === 'left' && !(c.pins && c.pins.length); });
  var _rSym = _allConns.filter(function (c) { return c.side === 'right' && !(c.pins && c.pins.length); });
  function connBadge(conn, cls) {
    return '<div class="' + cls + '">' + usbSvg + '<span>' + conn.label + '</span></div>';
  }
  var dip = '<div class="dbg-dip">';
  // Wrap the chip label, attaching edge-connector badges and the USB symbol
  // on the correct sides when needed.
  function wrapChip(chipHtml) {
    var left = _lSym.map(function (c) { return connBadge(c, 'dbg-usb-left'); }).join('');
    var right = _rSym.map(function (c) { return connBadge(c, 'dbg-usb-right'); }).join('')
      + (usbSide === 'right' ? usbElRight : '');
    if (!left && !right) return '<div class="dbg-chip">' + chipHtml + '</div>';
    return '<div class="dbg-chip-row">' + left + '<div class="dbg-chip">' + chipHtml + '</div>' + right + '</div>';
  }
  if (hasMultiCol) {
    // outer row first (col:2), then inner row (col:1), then chip, then inner, then outer
    dip += '<div class="dbg-col">' + colRow('left', 2) + '</div>';
    dip += '<div class="dbg-col">' + colRow('left', 1) + '</div>';
    dip += wrapChip(shortLabel);
    dip += '<div class="dbg-col">' + colRow('right', 1) + '</div>';
    dip += '<div class="dbg-col">' + colRow('right', 2) + '</div>';
  } else {
    // Pin-list connector blocks only (symbol badges are handled by wrapChip).
    var _lcArr = _allConns.filter(function (c) { return c.side === 'left' && c.pins && c.pins.length; });
    var _rcArr = _allConns.filter(function (c) { return c.side === 'right' && c.pins && c.pins.length; });
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
  var commentTip = dev.comment ? ' title="' + dev.comment.replace(/"/g, '&quot;') + '"' : '';
  html += '<span class="dbg-bus-id"' + commentTip + '>' + dev.id + '</span>';
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
      var mk = SERVO_MEANING[s.l];
      html += '<button class="dbg-hbtn ' + s.c + '" title="' + (mk ? t(mk) : s.l) + '" onclick="' + action + '">' + s.l + '</button>';
    });
  }
  html += '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '"'
    + ' onclick="openDevEditorById(\'' + sf + '\',' + boardApiIdx + ',1,SERVO_TYPES)">' + EDIT_ICO + '</button>';
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
// Linked-pins group highlight: hovering or clicking one tile of a multi-pin device
// lights up the whole set (a single-colour 3D lift — colour-blind friendly, no
// per-device hues). A click pins it for 3 s. _grpHi is the group id currently lit;
// renderPin re-applies it on every board re-render so a pinned highlight survives polling.
var _grpHi = null;
var _grpPinTimer = null;
var _grpDelegated = false;
function _grpApply() {
  var els = document.querySelectorAll('.dbg-pin[data-grp]');
  for (var i = 0; i < els.length; i++) {
    els[i].classList.toggle('grp-hi', !!_grpHi && els[i].getAttribute('data-grp') === _grpHi);
  }
}
function grpHi(id) { if (!_grpPinTimer) { _grpHi = id; _grpApply(); } }
function grpOut() { if (!_grpPinTimer) { _grpHi = null; _grpApply(); } }
function grpPin(id) {
  _grpHi = id; _grpApply();
  clearTimeout(_grpPinTimer);
  _grpPinTimer = setTimeout(function () { _grpPinTimer = null; _grpHi = null; _grpApply(); }, 3000);
}
// One delegated set of listeners (survives the board re-render on every poll).
function _grpInitDelegation() {
  if (_grpDelegated) return;
  _grpDelegated = true;
  document.addEventListener('mouseover', function (e) {
    var c = e.target.closest ? e.target.closest('.dbg-pin[data-grp]') : null;
    if (c) grpHi(c.getAttribute('data-grp'));
  });
  document.addEventListener('mouseout', function (e) {
    var c = e.target.closest ? e.target.closest('.dbg-pin[data-grp]') : null;
    if (!c) return;
    var to = e.relatedTarget && e.relatedTarget.closest ? e.relatedTarget.closest('.dbg-pin[data-grp]') : null;
    if (!to) grpOut(); // left the group (not moving to another grouped tile)
  });
  document.addEventListener('click', function (e) {
    var c = e.target.closest ? e.target.closest('.dbg-pin[data-grp]') : null;
    if (c) grpPin(c.getAttribute('data-grp'));
  });
}

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

  // Pins of a multi-pin device share a group key (the device id) so hovering or clicking
  // one tile highlights the whole set (see grpHi / grpPin). Single-pin → no group.
  var grp = (dev && dev.pins && dev.pins.length > 1) ? dev.id : null;

  // In DCC-label mode, a pin carrying a device shows "@<addr>", or "@-" when
  // that device has no DCC address configured (#129) — never silently falls
  // back to the GPIO number, which used to look like a DCC address itself.
  // In delay-label mode (#8), it shows the device's configured
  // start_delay_ms/_random_ms, e.g. "4.0s+2s", or "0s" when none is set
  // (#129) — same reasoning: a bare GPIO number there reads as a delay.
  // On SPI/I2C expansion boards pin.label is the only meaningful channel name
  // ("Q3", "CH7") so it's always kept; on plain GPIO boards it's a static
  // alt-function name ("TXD0") that's misleading once a device sits there
  // outside a bus — show the bare GPIO number instead, unless a bus currently
  // reserves the pin (_dbgSysPins), e.g. a device config-only-skipped by the
  // uart0 guard (#66) still shows "TXD0" while the log bus owns it (#70).
  var delayLabel = _dbgPinLabel === 'delay' && dev ? (fmtStartDelayLabel(dbgFindCfgDev(boardApiIdx, num)) || '0s') : null;
  var numLabel = _dbgPinLabel === 'dcc' && dev ? (dev.addr > 0 ? '@' + dev.addr : '@-')
    : delayLabel ? delayLabel
      : (isSpi || isI2c) ? pin.label
        : (dev ? (_dbgSysPins[num] || num) : pin.label);

  // Build a small LED toggle button for a free MCU GPIO (direct hardware test).
  function mkLedBtn(gpio) {
    var on = _dbgIdentify === ('g' + gpio);
    return '<button class="dbg-led-btn ' + (on ? 'on' : 'off') + '"'
      + ' onclick="event.stopPropagation();dbgIdentifyGpio(' + gpio + ')"'
      + ' title="' + t('dbg.identify_tip') + '">' + LED_ICO + '</button>';
  }

  // Build a small LED toggle button for a SPI expansion card channel (direct hardware test).
  function mkSpiLedBtn(card, ch) {
    var on = _dbgIdentify === ('c' + card + '_p' + ch);
    return '<button class="dbg-led-btn ' + (on ? 'on' : 'off') + '"'
      + ' onclick="event.stopPropagation();dbgIdentifySpi(' + card + ',' + ch + ')"'
      + ' title="' + t('dbg.identify_tip') + '">' + LED_ICO + '</button>';
  }

  var i2cTypeFilter = isI2c ? ',I2C_SERVO_TYPES.concat(I2C_MOTOR_TYPES)' : '';
  // Config-only comment/location note (#83) — appended to the icon tooltip when set.
  var devComment = dev ? (dbgFindCfgDev(boardApiIdx, num) || {}).comment : null;
  if (devComment) devComment = devComment.replace(/"/g, '&quot;');

  if (dev && dev._cfgOnly) {
    cls = 'cfg';
    var ico = ICONS[dev.type] || ICONS['_'];
    var tip = tooltip(dev.type) + (devComment ? ' — ' + devComment : '');
    inner = '<div class="dbg-pin-ico" title="' + tip + '">' + ico + '</div>'
      + '<span class="dbg-pin-num">' + numLabel + '</span>';
    editBtn = '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '" onclick="event.stopPropagation();openDevEditorById(\'' + dev.id + '\',' + boardApiIdx + ',' + num + i2cTypeFilter + ')">' + EDIT_ICO + '</button>';
  } else if (dev) {
    var isDevI2cServo = I2C_SERVO_TYPES.indexOf(dev.type) >= 0;
    var sc = dev.stateCount || 2;
    // Round-robin through every state for any multi-state device (signals,
    // I2C servos, …); plain on/off toggle for binary devices.
    var multi = isDevI2cServo || sc > 2;
    cls = dev.desired > 0 ? 'on' : 'off';
    if (multi) {
      onclick = ' onclick="dbgCycleDev(\'' + dev.id + '\',' + dev.desired + ',' + sc + ')"';
    } else {
      var ns = dev.desired > 0 ? 0 : 1;
      onclick = ' onclick="dbgToggleDev(\'' + dev.id + '\',' + ns + ')"';
    }
    var ico = ICONS[dev.type] || ICONS['_'];
    var tip = tooltip(dev.type) + (devComment ? ' — ' + devComment : '');
    var stateLabel = multi && dev.desired > 0 ? '<span class="dbg-pin-state">' + (isDevI2cServo ? 'P' : '') + dev.desired + '</span>' : '';
    inner = '<div class="dbg-pin-ico" title="' + tip + '">' + ico + '</div>'
      + '<span class="dbg-pin-num">' + numLabel + '</span>' + stateLabel;
    if (!isI2c) ledBtn = isSpi ? mkSpiLedBtn(board.spiRank, num) : mkLedBtn(num);
    editBtn = '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '" onclick="event.stopPropagation();openDevEditorById(\'' + dev.id + '\',' + boardApiIdx + ',' + num + i2cTypeFilter + ')">' + EDIT_ICO + '</button>';
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
      cls = _dbgIdentify === ('g' + num) ? 'test-on' : '';
      onclick = ' onclick="dbgIdentifyGpio(' + num + ')"';
      inner = '<span class="dbg-pin-num">' + num + '</span>';
      ledBtn = mkLedBtn(num);
      editBtn = '<button class="dbg-edit-btn" title="' + t('de.add_tip') + '" onclick="event.stopPropagation();openDevEditor(' + boardApiIdx + ',' + num + ',null)">+</button>';
    }
  } else {
    cls = _dbgIdentify === ('c' + board.spiRank + '_p' + num) ? 'test-on' : '';
    onclick = ' onclick="dbgIdentifySpi(' + board.spiRank + ',' + num + ')"';
    inner = '<span class="dbg-pin-num">' + num + '</span>';
    ledBtn = mkSpiLedBtn(board.spiRank, num);
    editBtn = '<button class="dbg-edit-btn" title="' + t('de.add_tip') + '" onclick="event.stopPropagation();openDevEditor(' + boardApiIdx + ',' + num + ',null)">+</button>';
  }

  var grpAttr = grp ? ' data-grp="' + grp + '"' : '';
  var grpHiCls = (grp && _grpHi === grp) ? ' grp-hi' : '';
  return '<div class="dbg-pin' + grpHiCls + (cls ? ' ' + cls : '') + '"' + grpAttr + onclick + '>' + inner + ledBtn + editBtn + '</div>';
}

// ── Actions ──────────────────────────────────────────────────────────

// Toggle a device on/off from the boards tab (uses /api/switch, then reloads).
function dbgToggleDev(id, on) {
  post('/api/switch', { id: id, on: !!on })
    .then(poll) // state-only change: refresh devices (RAM), not boards/config (flash) — avoids POV jitter
    .catch(function (e) { console.error('dbgToggleDev', e); });
}

// Cycle a multi-state device (e.g. PCA9685Servo) through its states on each click.
// desired=current state, stateCount=total states (0=STOP + N positions).
function dbgCycleDev(id, desired, stateCount) {
  var next = (desired + 1) % stateCount;
  // #8: instant response for a wiring test, not staggered like the cockpit.
  post('/api/device', { id: id, state: next, skip_delay: true })
    .then(poll) // state-only change: refresh devices (RAM), not boards/config (flash) — avoids POV jitter
    .catch(function (e) { console.error('dbgCycleDev', e); });
}

// Apply a UI theme (night | amber | signal | grey | dark | light); persists choice in localStorage.
function setTheme(name) {
  document.body.classList.remove('th-amber', 'th-signal', 'th-grey', 'th-dark', 'th-light');
  if (name !== 'night') document.body.classList.add('th-' + name);
  localStorage.setItem('mrj-theme', name);
  document.querySelectorAll('.theme-dot').forEach(function (b) {
    var match = b.classList.contains('theme-dot-' + name);
    b.classList.toggle('active', match);
  });
}

// Board view: switch pin-cell labels between the GPIO/channel number, the device's
// DCC address ("#<addr>"), and its configured startup delay (#8, "4.0s+2s").
// Persisted in localStorage; re-renders the pinout (RAM only).
var _dbgPinLabel = localStorage.getItem('mrj-pinlabel') || 'gpio';
function setPinLabel(mode) {
  _dbgPinLabel = (mode === 'dcc' || mode === 'delay') ? mode : 'gpio';
  localStorage.setItem('mrj-pinlabel', _dbgPinLabel);
  document.querySelectorAll('.pinlbl-btn').forEach(function (b) {
    b.classList.toggle('active', b.getAttribute('data-mode') === _dbgPinLabel);
  });
  if (_currentView === 'config') renderDebugBoards();
}

// Start (or stop, if already active) the recognizable "identify" blink on a GPIO,
// so the user can physically locate the connected LED. One target at a time.
function dbgIdentifyGpio(pin) {
  var key = 'g' + pin;
  var stop = _dbgIdentify === key;
  fetch('/api/test/identify', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(stop ? {} : { pin: pin })
  })
    .then(function (r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      _dbgIdentify = stop ? null : key;
      renderDebugBoards();
    })
    .catch(function (e) { console.error('dbgIdentifyGpio', e); });
}

// Same identify blink for a 74HC595 SPI channel.
function dbgIdentifySpi(card, channel) {
  var key = 'c' + card + '_p' + channel;
  var stop = _dbgIdentify === key;
  fetch('/api/test/identify', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(stop ? {} : { card: card, channel: channel })
  })
    .then(function (r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      _dbgIdentify = stop ? null : key;
      renderDebugBoards();
    })
    .catch(function (e) { console.error('dbgIdentifySpi', e); });
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
  // Stop a running pin identify (triple-blink) too, so ALL OFF/ON leaves nothing testing.
  if (_dbgIdentify) {
    clearCalls.push(fetch('/api/test/identify', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({})
    }));
    _dbgIdentify = null;
  }
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
    // #8: instant response for a wiring test, not staggered like the cockpit.
    post('/api/all', { state: state, board: boardApiIdx + 1, skip_delay: true })
      .then(poll); // refresh devices only (RAM), not boards/config (flash) — avoids POV jitter
  });
}

// ── Idle pins ────────────────────────────────────────────────────────

/**
 * Compute free output GPIO pins on the root MCU board for a given config.
 * Called automatically by saveCfg() — result is stored as cfg.idle_pins so
 * the firmware drives them OUTPUT LOW at boot without any manual step.
 *
 * Excluded: system/bus pins (_dbgSysPins), strapping pins,
 *           input-only pins, and pins already used by a device or bus in cfg.
 *
 * @param {object} cfg  Config object to inspect (falls back to _dbgCfg).
 * @returns {number[]} Sorted array of GPIO numbers to drive OUTPUT LOW.
 */
function computeIdlePins(cfg) {
  cfg = cfg || _dbgCfg;
  if (!cfg) return [];
  // Find root board (first board without a bus).
  var rootBoard = null;
  var cfgBoards = cfg.boards || [];
  for (var i = 0; i < cfgBoards.length; i++) {
    if (!cfgBoards[i].bus) { rootBoard = cfgBoards[i]; break; }
  }
  if (!rootBoard) return [];
  var def = _boardTypes[rootBoard.type];
  if (!def || !def.pins) return [];

  // Collect all GPIO numbers already committed: device wiring + bus signal pins.
  var usedPins = {};
  (cfg.devices || []).forEach(function (dev) {
    var w = dev.wiring;
    if (Array.isArray(w)) w.forEach(function (p) { usedPins[p] = true; });
    else if (w !== undefined && w !== null) usedPins[w] = true;
  });
  var buses = cfg.buses || {};
  Object.keys(buses).forEach(function (k) {
    var b = buses[k];
    ['pin', 'mosi', 'sclk', 'latch', 'tx', 'rx', 'sda', 'scl'].forEach(function (f) {
      if (b[f] !== undefined) usedPins[b[f]] = true;
    });
  });

  var idle = [];
  def.pins.forEach(function (p) {
    if (p.wiring === undefined) return;
    var num = p.wiring;
    var caps = p.capabilities || [];
    if (caps.indexOf('output') < 0) return;       // not an output pin
    if (caps.indexOf('strapping') >= 0) return;    // boot-strapping pin — skip
    if (_dbgSysPins[num]) return;                  // reserved by a bus (runtime)
    if (usedPins[num]) return;                     // already used by a device or bus
    idle.push(num);
  });
  idle.sort(function (a, b) { return a - b; });
  return idle;
}

