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
      var cfgLookup = _deCfgDevById(id);
      var mDev = {
        id: rtDev.id, type: rtDev.type, board: rtDev.board,
        desired: rtDev.desired, state: rtDev.state, addr: rtDev.addr,
        pins: rtDev.pins, label: rtDev.label
      };
      if (cfgLookup) {
        if (cfgLookup.angle_a !== undefined) mDev.angle_a = cfgLookup.angle_a;
        if (cfgLookup.angle_b !== undefined) mDev.angle_b = cfgLookup.angle_b;
        if (cfgLookup.positions !== undefined) mDev.positions = cfgLookup.positions;
        if (cfgLookup.pulse_min_us !== undefined) mDev.pulse_min_us = cfgLookup.pulse_min_us;
        if (cfgLookup.pulse_max_us !== undefined) mDev.pulse_max_us = cfgLookup.pulse_max_us;
        if (cfgLookup.speed !== undefined) mDev.speed = cfgLookup.speed;
        if (cfgLookup.states !== undefined) mDev.states = cfgLookup.states;
        if (cfgLookup.neutral_us !== undefined) mDev.neutral_us = cfgLookup.neutral_us;
        // default_state = persisted boot state — read from config, never from runtime desired.
        if (cfgLookup.default_state !== undefined) mDev.default_state = cfgLookup.default_state;
      }
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
    document.getElementById('de-defstate').value = (dev.default_state === 'on') ? 'on' : '';
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
  var html = '<div class="de-field"><label>' + wiringLabel + '</label><div class="de-wiring-row">';

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
