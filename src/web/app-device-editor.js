/**
 * @file app-device-editor.js
 * @brief Device editor modal for creating and editing individual devices in the WebUI.
 *
 * Handles device creation/modification with pin selection, type picker, and parameter
 * configuration. Supports both GPIO and bus-based devices (I2C, SPI).
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

var _deEditId = null;    // id of the device currently being edited, null = new
var _deFixedPin = undefined;          // pin/channel locked from context (pin click), undefined = free
var _deFixedBoardApiIdx = undefined;  // board index locked from context (I2C boards)
var _deTypeFilter = null;             // explicit type-list override (e.g. SERVO_TYPES), else by-board

// Pin-count hint shown in the type picker. Serial servos use a bus ID, not GPIO pins.
function _dePinHint(tp) {
  var dt = _deviceTypes[tp] || {};
  if (dt.category === 'servo') return t('de.bus_id');
  var n = dt.wires !== undefined ? dt.wires : 1;
  return n + ' ' + (n === 1 ? t('de.pin_one') : t('de.pins'));
}

// Device types a board can host, by its bus kind: GPIO/SPI = digital output effects,
// I2C = PCA9685 servo/motor, UART = serial servo / audio.
function _deAllowedTypes(boardIdx) {
  var b = (boardIdx !== undefined && _dbgBoards[boardIdx]) ? _dbgBoards[boardIdx] : null;
  var bt = b ? (_boardTypes[b.type] || {}) : {};
  var busType = (b && b.spiRank > 0) ? 'spi' : bt.busType; // null/undefined = GPIO
  var allow;
  if (busType === 'i2c') allow = ['i2c_servo', 'i2c_motor'];
  else if (busType === 'uart') allow = ['servo', 'audio'];
  else allow = ['light', 'signal', 'traffic', 'static']; // GPIO + SPI digital outputs
  return Object.keys(_deviceTypes).filter(function (k) {
    return allow.indexOf((_deviceTypes[k] || {}).category) >= 0;
  });
}

// (Re)build the type dropdown — filtered to the board capability (or _deTypeFilter).
// forceType: the type being edited, which must stay selectable & selected even if
// the board filter excludes it. For a NEW device pass null → the list resets to the
// board's first allowed type (never carries over the previously-open device's type).
function deRenderTypeList(typeFilter, forceType, boardIdx) {
  var typeEl = document.getElementById('de-type');
  if (!typeEl) return;
  if (boardIdx === undefined) {
    var be = document.getElementById('de-board');
    boardIdx = (be && be.value !== '') ? parseInt(be.value, 10) : _deFixedBoardApiIdx;
  }
  var typeList = typeFilter || _deAllowedTypes(boardIdx);
  if (forceType && typeList.indexOf(forceType) < 0) typeList = [forceType].concat(typeList);
  if (!typeList.length) typeList = Object.keys(_deviceTypes); // safety net
  // Hidden <select> holds the value (read by all the existing code); the rich,
  // icon-carrying options live in the custom panel below.
  typeEl.innerHTML = typeList.map(function (tp) {
    return '<option value="' + tp + '">' + tp + '</option>';
  }).join('');
  typeEl.value = forceType || (typeList[0] || '');
  typeEl.disabled = typeList.length === 1;
  var panel = document.getElementById('de-type-panel');
  if (panel) {
    panel.innerHTML = typeList.map(function (tp) {
      return '<div class="de-type-opt" data-tp="' + tp + '" onclick="deSelectType(\'' + tp + '\')">'
        + '<span class="de-type-opt-ico">' + (ICONS[tp] || ICONS['_'] || '') + '</span>'
        + '<span class="de-type-opt-name">' + tp + ' — ' + tooltip(tp) + '</span>'
        + '<span class="de-type-opt-pins">' + _dePinHint(tp) + '</span>'
        + '</div>';
    }).join('');
  }
  deSyncTypeTrigger();
}

// Update the picker trigger (icon + name) from the hidden <select>'s current value.
function deSyncTypeTrigger() {
  var trg = document.getElementById('de-type-trigger');
  var typeEl = document.getElementById('de-type');
  if (!trg || !typeEl) return;
  var tp = typeEl.value;
  trg.innerHTML = '<span class="de-type-opt-ico">' + (ICONS[tp] || ICONS['_'] || '') + '</span>'
    + '<span class="de-type-opt-name">' + (tp ? tp + ' — ' + tooltip(tp) : '') + '</span>'
    + '<span class="de-type-caret">▾</span>';
  trg.disabled = typeEl.disabled;
}

// Pick a type from the custom panel: set the hidden <select>, close, fire its change.
function deSelectType(tp) {
  var typeEl = document.getElementById('de-type');
  if (!typeEl) return;
  typeEl.value = tp;
  deCloseTypeDd();
  deSyncTypeTrigger();
  typeEl.dispatchEvent(new Event('change')); // runs deUpdateWiring/… exactly as before
}

// Open/close the custom type panel.
function deToggleTypeDd(event) {
  if (event) event.stopPropagation();
  var typeEl = document.getElementById('de-type');
  if (typeEl && typeEl.disabled) return; // single option → non-interactive
  var panel = document.getElementById('de-type-panel');
  if (!panel) return;
  if (panel.style.display !== 'none') { deCloseTypeDd(); return; }
  panel.style.display = '';
  var cur = typeEl ? typeEl.value : '';
  var opts = panel.querySelectorAll('.de-type-opt');
  for (var i = 0; i < opts.length; i++) {
    opts[i].classList.toggle('sel', opts[i].getAttribute('data-tp') === cur);
    opts[i].classList.remove('hl');
  }
  setTimeout(function () {
    document.addEventListener('click', deCloseTypeDd);
    document.addEventListener('keydown', deTypeDdKey);
  }, 0);
}
function deCloseTypeDd() {
  var panel = document.getElementById('de-type-panel');
  if (panel) panel.style.display = 'none';
  document.removeEventListener('click', deCloseTypeDd);
  document.removeEventListener('keydown', deTypeDdKey);
}
// Keyboard: ↑/↓ move the highlight, Enter selects, Esc closes.
function deTypeDdKey(e) {
  var panel = document.getElementById('de-type-panel');
  if (!panel || panel.style.display === 'none') return;
  if (e.key === 'Escape') { deCloseTypeDd(); return; }
  var opts = panel.querySelectorAll('.de-type-opt');
  if (!opts.length) return;
  var hi = -1;
  for (var i = 0; i < opts.length; i++) if (opts[i].classList.contains('hl')) { hi = i; break; }
  if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
    e.preventDefault();
    var ni = e.key === 'ArrowDown' ? (hi < 0 ? 0 : Math.min(hi + 1, opts.length - 1))
                                   : (hi < 0 ? opts.length - 1 : Math.max(hi - 1, 0));
    for (var j = 0; j < opts.length; j++) opts[j].classList.toggle('hl', j === ni);
    opts[ni].scrollIntoView({ block: 'nearest' });
  } else if (e.key === 'Enter') {
    e.preventDefault();
    if (hi >= 0) deSelectType(opts[hi].getAttribute('data-tp'));
  }
}

// Board changed in the editor → refilter the type list to the new board's capability
// (resets to the first allowed type for that board).
function deOnBoardChange() {
  deRenderTypeList(_deTypeFilter, null);
}

// Look up a device entry in the persisted config (_dbgCfg) by id, or null.
// _dbgCfg is the source of truth for default_state (the boot state), which is
// NOT reported by the runtime /api/devices endpoint.
function _deCfgDevById(id) {
  if (_dbgCfg && _dbgCfg.devices) {
    for (var i = 0; i < _dbgCfg.devices.length; i++) {
      if (_dbgCfg.devices[i].id === id) return _dbgCfg.devices[i];
    }
  }
  return null;
}

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
    // Carry config-sourced servo/motor params (from _busDev) so editing then
    // re-saving a bus device does NOT silently drop them (neutral_us, states…).
    if (bd.positions !== undefined) merged.positions = bd.positions;
    if (bd.pulse_min_us !== undefined) merged.pulse_min_us = bd.pulse_min_us;
    if (bd.pulse_max_us !== undefined) merged.pulse_max_us = bd.pulse_max_us;
    if (bd.speed !== undefined) merged.speed = bd.speed;
    if (bd.states !== undefined) merged.states = bd.states;
    if (bd.neutral_us !== undefined) merged.neutral_us = bd.neutral_us;
    if (bd.angle_a !== undefined) merged.angle_a = bd.angle_a;
    if (bd.angle_b !== undefined) merged.angle_b = bd.angle_b;
    // default_state = persisted boot state — read from config, never from runtime desired.
    var cfgBd = _deCfgDevById(id);
    if (cfgBd && cfgBd.default_state !== undefined) merged.default_state = cfgBd.default_state;
    openDevEditor(boardApiIdx, pin, merged, typeFilter);
    return;
  }
  // Non-bus devices: use runtime dev, merging config-only fields (angle_a, angle_b, etc.)
  for (var j = 0; j < _dbgDevs.length; j++) {
    if (_dbgDevs[j].id === id) {
      var rtDev = _dbgDevs[j];
      // Overlay the persisted config (source of truth for servo/motor params +
      // default_state) onto the runtime device. See app-pure.js / test/web/.
      var mDev = mergeDeviceForEditor(rtDev, _deCfgDevById(id));
      openDevEditor(boardApiIdx, pin, mDev, typeFilter);
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
          // Full wiring from config (not just the clicked pin) so a multi-pin device
          // edited before the firmware reloads still shows ALL its pins.
          pins: Array.isArray(cfgDev.wiring) ? cfgDev.wiring.slice()
              : (cfgDev.wiring !== undefined ? [cfgDev.wiring] : [pin]),
          label: cfgDev.label
        };
        if (cfgDev.positions !== undefined) cfgOnlyDev.positions = cfgDev.positions;
        if (cfgDev.pulse_min_us !== undefined) cfgOnlyDev.pulse_min_us = cfgDev.pulse_min_us;
        if (cfgDev.pulse_max_us !== undefined) cfgOnlyDev.pulse_max_us = cfgDev.pulse_max_us;
        if (cfgDev.speed !== undefined) cfgOnlyDev.speed = cfgDev.speed;
        if (cfgDev.states !== undefined) cfgOnlyDev.states = cfgDev.states;
        if (cfgDev.neutral_us !== undefined) cfgOnlyDev.neutral_us = cfgDev.neutral_us;
        if (cfgDev.angle_a !== undefined) cfgOnlyDev.angle_a = cfgDev.angle_a;
        if (cfgDev.angle_b !== undefined) cfgOnlyDev.angle_b = cfgDev.angle_b;
        if (cfgDev.default_state !== undefined) cfgOnlyDev.default_state = cfgDev.default_state;
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

  // Type select — filtered to the context board's capability (or an explicit typeFilter),
  // keeping the edited device's type selectable. Shows pin count + icon per option.
  var typeEl = document.getElementById('de-type');
  _deTypeFilter = typeFilter || null;
  deRenderTypeList(_deTypeFilter, dev ? dev.type : null, _deFixedBoardApiIdx);

  // Board select
  var boardEl = document.getElementById('de-board');
  boardEl.innerHTML = _dbgBoards.map(function (b, i) {
    var badge = b.spiRank > 0 ? 'SPI board ' + b.spiRank : 'GPIO';
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
    // Boot state comes from the persisted config (default_state), NOT the current
    // runtime state (desired). Using desired here silently baked default_state:"on"
    // into the config whenever a device was edited while running (e.g. a tested motor).
    // The default-state select itself is (re)built by deUpdateDefState(dev) below.
    document.getElementById('de-del-btn').style.display = '';
  } else {
    document.getElementById('de-title').textContent = t('de.new');
    document.getElementById('de-id').value = '';
    if (boardApiIdx !== undefined) boardEl.value = boardApiIdx;
    document.getElementById('de-addr').value = '';
    document.getElementById('de-del-btn').style.display = 'none';
  }

  deUpdateWiring(prefillPin, dev);
  deUpdateAddrLabel();
  deUpdateDefState(dev);
  deStatus('', '');
  document.getElementById('de-save-btn').disabled = false;

  document.getElementById('de-overlay').style.display = 'block';
  deResetModalPos();
  document.getElementById('de-modal').style.display = 'flex';
  applyLang();
}

// Close the device editor modal without saving.
function closeDevEditor() {
  deWireTestStop();
  document.getElementById('de-overlay').style.display = 'none';
  document.getElementById('de-modal').style.display = 'none';
}

// ── DB-signal wiring assistant ───────────────────────────────────────────────
var _deWireTesting = null; // index of the pin position currently being identified

// Charlieplex test of the pin selected in dropdown `idx`: blink it while holding
// the other selected wires LOW, so one LED lights even before the device is saved.
// Clicking the active test again stops it.
function deWireTest(idx) {
  var sels = document.querySelectorAll('#de-wiring-grp .de-w');
  var pin = parseInt(sels[idx] && sels[idx].value, 10);
  if (isNaN(pin)) { deStatus(t('de.err_wire_nopin'), 'err'); return; }
  var stop = _deWireTesting === idx;
  var low = [];
  if (!stop) {
    for (var i = 0; i < sels.length; i++) {
      if (i === idx) continue;
      var p = parseInt(sels[i].value, 10);
      if (!isNaN(p) && p !== pin) low.push(p);
    }
  }
  post('/api/test/identify', stop ? {} : { pin: pin, low: low })
    .then(function (r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      _deWireTesting = stop ? null : idx;
      var btns = document.querySelectorAll('#de-wiring-grp .de-wire-test');
      for (var i = 0; i < btns.length; i++) btns[i].classList.toggle('on', _deWireTesting === i);
      deStatus('', '');
    })
    .catch(function (e) { console.error('deWireTest', e); deStatus(t('de.err_wire_test'), 'err'); });
}

// Stop any active wiring-test blink (called on editor close / after save).
function deWireTestStop() {
  if (_deWireTesting === null) return;
  _deWireTesting = null;
  post('/api/test/identify', {}).catch(function () {});
}

// ── Draggable editor modal (grab the header bar) ─────────────────────────────
var _deOffX = 0, _deOffY = 0, _deDrag = null;

function deDragStart(e) {
  if (e.target.closest('.de-close')) return; // ✕ button: don't start a drag
  _deDrag = { sx: e.clientX, sy: e.clientY, ox: _deOffX, oy: _deOffY };
  document.addEventListener('mousemove', deDragMove);
  document.addEventListener('mouseup', deDragEnd);
  e.preventDefault();
}
function deDragMove(e) {
  if (!_deDrag) return;
  _deOffX = _deDrag.ox + (e.clientX - _deDrag.sx);
  _deOffY = _deDrag.oy + (e.clientY - _deDrag.sy);
  document.getElementById('de-modal').style.transform =
    'translate(calc(-50% + ' + _deOffX + 'px), calc(-50% + ' + _deOffY + 'px))';
}
function deDragEnd() {
  _deDrag = null;
  document.removeEventListener('mousemove', deDragMove);
  document.removeEventListener('mouseup', deDragEnd);
}

// Re-center the modal (clear any drag offset) — called when (re)opening the editor.
function deResetModalPos() {
  _deOffX = _deOffY = 0;
  var m = document.getElementById('de-modal');
  if (m) m.style.transform = '';
}

// Keyboard shortcuts for the editor modals: ESC = Cancel, Enter = Save.
// Covers all three editors (device, board, bus).
(function () {
  var modals = [
    { id: 'de-modal',  close: function () { closeDevEditor(); },   save: function () { saveDevEditor(); } },
    { id: 'be-modal',  close: function () { closeBoardEditor(); }, save: function () { saveBoardEditor(); } },
    { id: 'bue-modal', close: function () { closeBusEditor(); },   save: function () { saveBusEditor(); } }
  ];
  document.addEventListener('keydown', function (e) {
    if (e.key !== 'Escape' && e.key !== 'Enter') return;
    var open = null;
    for (var i = 0; i < modals.length; i++) {
      var el = document.getElementById(modals[i].id);
      if (el && el.style.display && el.style.display !== 'none') { open = modals[i]; break; }
    }
    if (!open) return;
    if (e.key === 'Escape') { open.close(); return; }
    // Enter = Save, but let a focused button (Cancel/Delete/Save) or textarea act normally.
    var ae = document.activeElement;
    if (ae && (ae.tagName === 'BUTTON' || ae.tagName === 'TEXTAREA')) return;
    e.preventDefault();
    open.save();
  });
})();

// Update addr/board field visibility and labels based on the currently selected device type.
// Servo devices (UART): board selector hidden (bus board is implicit from the chain).
// I2C servo devices: board selector hidden (board and channel are fixed from pin click context).
function deUpdateAddrLabel() {
  var type = document.getElementById('de-type').value;
  var isServo = SERVO_TYPES.indexOf(type) >= 0;
  var isI2cServo = I2C_SERVO_TYPES.indexOf(type) >= 0;
  var isI2cMotor = I2C_MOTOR_TYPES.indexOf(type) >= 0;
  // Board field: the board is always fixed by the context (the board/pin that was
  // clicked), so hide this redundant picker. It used to show for GPIO devices, where
  // it served no purpose and — with the type refilter on change — was disruptive.
  var boardField = document.getElementById('de-board-field');
  if (boardField) boardField.style.display = (_deFixedBoardApiIdx !== undefined) ? 'none' : '';
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
// Rebuild the "default state" select for the current device type. Multi-state
// types (signals, servos) list every state from the catalog (OFF/HP0/HP1/…);
// binary devices keep off/on. Pre-selects dev's stored default_state.
// Called on modal open (dev provided) and on type change (no args).
function deUpdateDefState(dev) {
  var sel = document.getElementById('de-defstate');
  if (!sel) return;
  var type = document.getElementById('de-type').value;
  var states = (_deviceTypes[type] || {}).states;
  var v = deDefaultStateValue(dev); // '', 'on', or a numeric-string state value
  if (states && states.length) {
    // Map legacy 'on'/'off' onto state values; default to the first state (OFF).
    var sv = v === 'on' ? '1' : (v === '' ? String(states[0].v) : v);
    sel.innerHTML = states.map(function (s) {
      return '<option value="' + s.v + '"' + (String(s.v) === sv ? ' selected' : '') + '>' + s.l + '</option>';
    }).join('');
  } else {
    sel.innerHTML = '<option value=""' + (v === '' ? ' selected' : '') + '>off</option>'
      + '<option value="on"' + (v === 'on' ? ' selected' : '') + '>on</option>';
  }
}

// I2C servo (PCA9685): channel is fixed from pin-click context — shown as a badge.
// GPIO/SPI: <select> dropdown filtered to available output pins.
// Called on modal open (prefillPin/dev provided) and on type/board change (no args).
function deUpdateWiring(prefillPin, dev) {
  var type = document.getElementById('de-type').value;
  var isServo = SERVO_TYPES.indexOf(type) >= 0;
  var isI2cServo = I2C_SERVO_TYPES.indexOf(type) >= 0;
  var isI2cMotor = I2C_MOTOR_TYPES.indexOf(type) >= 0;
  var count = (_deviceTypes[type] || {}).wires !== undefined ? (_deviceTypes[type] || {}).wires : 1;
  // Serial servos declare wires:0 (they live on a UART bus, not a GPIO pin) but the
  // editor still needs one field for the bus ID — force a single input.
  if (isServo && count < 1) count = 1;
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
      + '<input type="number" class="de-mst-up"  min="0" max="30000"  value="' + (rampUp || 0) + '" style="width:4rem" title="' + t('de.mst_up_tip') + '">'
      + '<input type="number" class="de-mst-dur" min="0" max="300000" value="' + (dur !== undefined ? dur : 0) + '" style="width:5rem" title="' + t('de.mst_dur_tip') + '">'
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
        + '<span style="width:4rem">↑ ms</span>'
        + '<span style="width:5rem">Durée ms</span>'
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
      + '<div class="de-wiring-row"><span class="de-wiring-fixed">CH ' + _deFixedPin + '</span>'
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
  // DB signals expose per-position "wire_aspects" → show the wiring assistant
  // (a Test button + aspect radios under each pin); pins are reordered on save.
  var wa = !isServo ? ((_deviceTypes[type] || {}).wire_aspects || null) : null;
  // showTest: GPIO output device on a GPIO board → per-pin Test button (identify
  // the wire). wa (DB signals, traffic lights) adds aspect radios + reorder on save.
  var _wBoard = _dbgBoards[parseInt(document.getElementById('de-board').value, 10)];
  var _wBt = _wBoard ? _boardTypes[_wBoard.type] : null;
  var showTest = !isServo && !isI2cServo && !isI2cMotor && !(_wBt && _wBt.busType);
  var html = '<div class="de-field"><label>' + wiringLabel + '</label>';
  if (showTest) html += '<div class="de-wire-hint">' + t(wa ? 'de.wire_hint' : 'de.wire_hint_test') + '</div>';
  html += '<div class="' + (showTest ? 'de-wiring-col' : 'de-wiring-row') + '">';

  if (isServo) {
    for (var i = 0; i < count; i++) {
      html += '<input type="number" class="de-w" min="1" max="253"'
        + ' placeholder="ID' + (count > 1 ? ' ' + (i + 1) : '') + '"'
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
      if (showTest) html += '<div class="de-wire-block"><div class="de-wire-head">';
      html += '<select class="de-w" onchange="deUpdateIdPlaceholder()">';
      html += '<option value="">— pin' + selLabel + ' —</option>';
      availOpts.forEach(function (o) {
        if (o.used && o.val !== curVal) return;
        var sel = (o.val === curVal) ? ' selected' : '';
        html += '<option value="' + o.val + '"' + sel + '>' + o.val + '</option>';
      });
      html += '</select>';
      if (showTest) {
        html += '<button type="button" class="de-wire-test" onclick="deWireTest(' + i + ')">'
          + t('de.wire_test') + '</button></div>';
        if (wa) {
          html += '<div class="de-wire-aspects">';
          wa.forEach(function (a, ai) {
            // On edit, pins are stored in canonical order → slot i shows aspect i,
            // so pre-check that radio to reflect the saved wiring.
            var chk = (dev && ai === i) ? ' checked' : '';
            html += '<label class="de-wire-asp"><input type="radio" name="de-wa-' + i + '" value="' + ai + '"' + chk + '>'
              + '<span class="de-wire-sw" style="background:' + (a.c || '#888') + '"></span>'
              + t('wa.' + a.l) + '</label>';
          });
          html += '</div>';
        }
        html += '</div>';
      }
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
    + '<input type="number" class="de-mst-up"  min="0" max="30000"  value="0" style="width:4rem" title="' + t('de.mst_up_tip') + '">'
    + '<input type="number" class="de-mst-dur" min="0" max="300000" value="0" style="width:5rem" title="' + t('de.mst_dur_tip') + '">'
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
  if (isServo && count < 1) count = 1; // serial servo: the single wiring value is the bus ID
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

  // Wiring assistant: if the user answered the aspect radios, reorder the pins so
  // each lands at the position whose canonical aspect matches what they observed.
  var waSave = (_deviceTypes[type] || {}).wire_aspects;
  if (waSave && wiring.length > 1) {
    var chosen = [], anyChecked = false;
    for (var wi = 0; wi < wiring.length; wi++) {
      var r = document.querySelector('input[name="de-wa-' + wi + '"]:checked');
      chosen.push(r ? parseInt(r.value, 10) : -1);
      if (r) anyChecked = true;
    }
    if (anyChecked) {
      var seen = {};
      for (var wj = 0; wj < wiring.length; wj++) {
        if (chosen[wj] < 0 || chosen[wj] >= wiring.length || seen[chosen[wj]]) {
          deStatus(t('de.err_wire_aspects'), 'err');
          document.getElementById('de-save-btn').disabled = false;
          return;
        }
        seen[chosen[wj]] = true;
      }
      var reordered = new Array(wiring.length);
      for (var wk = 0; wk < wiring.length; wk++) reordered[chosen[wk]] = wiring[wk];
      wiring = reordered;
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
  if (defState) {
    var dsn = parseInt(defState, 10);
    dev.default_state = isNaN(dsn) ? defState : dsn; // number for state values, 'on' for binary
  }

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
