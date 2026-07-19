/**
 * @file app-wizard.js
 * @brief First-boot setup wizard and configuration utilities.
 *
 * Provides initial setup wizard, manual configuration reset, and code export
 * functionality for the WebUI.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

// ── Setup wizard (first boot) ────────────────────────────────────────────────
// Replaces the old static welcome modal.  Triggered when the first /api/devices
// poll returns an empty list AND no boards are configured yet.
// The wizard collects board, buses and expansion cards in 3–4 steps, builds the
// config in memory, saves it once, and calls POST /api/reload — no restart.

var _wiz = null;          // null = wizard closed
var _wizExpIdx = 0;       // monotonic counter for expansion-board _idx

/**
 * @brief Get default I2C address for a board type.
 * Uses i2c_known.json (address → device types) to find the first matching address.
 * @param boardType Board type key (e.g. "OLED SSD1306 128x64", "PCA9685")
 * @return Default I2C address (decimal), or 64 if not found
 */
function _wizGetDefaultI2cAddress(boardType) {
  if (!boardType) return 64;

  // Normalize board type for matching (remove special chars, case-insensitive)
  var btNorm = boardType.toLowerCase().replace(/[^a-z0-9]/g, '');

  // Search i2c_known for first address that matches this board type
  for (var addr in _i2cKnown) {
    var devices = _i2cKnown[addr].toLowerCase();
    // Check if board type appears in the device list (e.g. "ssd1306" in "SSD1306/SH1106")
    if (devices.indexOf(btNorm.substring(0, 7)) !== -1 || // prefix match (e.g. "ssd1306")
        devices.indexOf(btNorm.substring(0, 6)) !== -1 || // (e.g. "pca968")
        devices.indexOf(btNorm.substring(0, 5)) !== -1) { // (e.g. "pcf85")
      return parseInt(addr, 10);
    }
  }

  // Fallback defaults based on common patterns
  if (btNorm.indexOf('oled') !== -1 || btNorm.indexOf('ssd1306') !== -1 || btNorm.indexOf('sh1106') !== -1) {
    return 60; // 0x3C
  }
  if (btNorm.indexOf('pca9685') !== -1) {
    return 64; // 0x40
  }
  if (btNorm.indexOf('pcf8574a') !== -1) {
    return 56; // 0x38
  }
  if (btNorm.indexOf('pcf8574') !== -1) {
    return 32; // 0x20
  }

  return 64; // Default fallback
}

// Entry point — called by showWelcome() after config + status + board-types are loaded.
function _wizOpen(status) {
  var env = ((status && status.env) || '').toLowerCase().replace(/[_\s-]/g, '');
  var defaultType = _ENV_TO_BOARD[env] || guessDefaultBoardType();
  var boardId = defaultType.toLowerCase().replace(/[^a-z0-9]/g, '');

  _wizExpIdx = 0;
  _wiz = {
    step: 0,
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

// How many steps total (step 0=lang always present; step 3=expansion skipped when no buses).
function _wizTotalSteps() {
  if (!_wiz) return 5;
  return (_wiz.buses.i2c.enabled || _wiz.buses.spi.enabled || _wiz.buses.uart.enabled) ? 5 : 4;
}

// Map internal step (0-4) to display step (1-based); handles expansion skip.
function _wizDisplayStep(step) {
  return (step === 4 && _wizTotalSteps() === 4) ? 4 : step + 1;
}

// ── Step renderers ───────────────────────────────────────────────────────────

function _wizRenderStep0() {
  var langs = [
    { code: 'fr', label: 'Français' },
    { code: 'en', label: 'English' },
    { code: 'de', label: 'Deutsch' },
    { code: 'es', label: 'Español' }
  ];
  return '<p style="margin:0 0 .5rem;font-size:.9rem;color:var(--t2)">' + t('wiz.lang_intro') + '</p>'
    + '<div class="wiz-lang-grid">'
    + langs.map(function (l) {
      var active = (typeof _lang !== 'undefined' && _lang === l.code) ? ' wiz-lang-active' : '';
      return '<button class="wiz-lang-btn' + active + '" onclick="wizSetLang(\'' + l.code + '\')">' + l.label + '</button>';
    }).join('')
    + '</div>';
}

function wizSetLang(lang) {
  if (typeof setLang === 'function') setLang(lang);
}

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
    + '<label>' + t('wiz.board_type_lbl') + '</label>'
    + '<select id="wiz-board-type" onchange="wizUpdateBoardId()">' + opts + '</select>'
    + '</div>'
    + '<div class="de-field">'
    + '<label>' + t('wiz.board_id_lbl') + '</label>'
    + '<input type="text" id="wiz-board-id" value="' + _wiz.mainBoardId + '" autocomplete="off" placeholder="' + t('wiz.board_id_ph') + '">'
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
  html += _wizBusCard('uart', t('wiz.uart_label'), [
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
    + '<label style="font-size:.8rem">' + t('wiz.i2c_addr_lbl') + '</label>'
    + '<input type="number" value="' + b.i2cAddress + '" min="0" max="127" '
    + 'onchange="wizUpdateExpBoard(' + b._idx + ',\'i2cAddress\',+this.value)">'
    + '</div>'
    : '';
  return '<div class="wiz-expboard-row">'
    + '<button class="wiz-del-board" onclick="wizDeleteExpBoard(' + b._idx + ')">✕</button>'
    + '<div class="de-field">'
    + '<label style="font-size:.8rem">' + t('wiz.board_type_lbl') + '</label>'
    + '<select onchange="wizUpdateExpBoard(' + b._idx + ',\'type\',this.value)">' + typeOpts + '</select>'
    + '</div>'
    + '<div class="de-field">'
    + '<label style="font-size:.8rem">ID</label>'
    + '<input type="text" value="' + b.id + '" placeholder="' + t('wiz.exp_id_ph') + '" '
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
    + t('wiz.exp_add') + '</button>'
    + '</div>';
}

function _wizRenderStep3() {
  var html = '';
  if (_wiz.buses.i2c.enabled) html += _wizRenderExpSection('i2c', 'Bus I²C (' + _wiz.buses.i2c.key + ')');
  if (_wiz.buses.spi.enabled) html += _wizRenderExpSection('spi', 'Bus SPI (' + _wiz.buses.spi.key + ')');
  if (_wiz.buses.uart.enabled) html += _wizRenderExpSection('uart', t('wiz.uart_label') + ' (' + _wiz.buses.uart.key + ')');
  if (!html) {
    html = '<p style="color:var(--t3);font-size:.85rem">' + t('wiz.exp_none') + '</p>';
  }
  return html;
}

function _wizRenderStep4() {
  var lines = [];
  var bt = _boardTypes[_wiz.mainBoardType];
  lines.push('<strong>' + t('wiz.board_lbl') + '</strong> ' + ((bt && bt.label) || _wiz.mainBoardType)
    + ' <span style="color:var(--t3)">(id: ' + _wiz.mainBoardId + ')</span>');
  var b = _wiz.buses;
  if (b.i2c.enabled) lines.push('<strong>I²C :</strong> SDA=' + b.i2c.sda + ', SCL=' + b.i2c.scl);
  if (b.spi.enabled) lines.push('<strong>SPI :</strong> MOSI=' + b.spi.mosi + ', SCLK=' + b.spi.sclk + ', LATCH=' + b.spi.latch);
  if (b.uart.enabled) lines.push('<strong>' + t('wiz.serial_lbl') + '</strong> TX=' + b.uart.tx + ', RX=' + b.uart.rx + ', Baud=' + b.uart.baud);
  _wiz.expansionBoards.forEach(function (eb) {
    var def = _boardTypes[eb.type] || {};
    lines.push('<strong>' + t('wiz.ext_lbl') + '</strong> ' + (def.label || eb.type)
      + ' <span style="color:var(--t3)">(id: ' + eb.id + ', bus: ' + eb.busKey + ')</span>');
  });
  var ph = t('wiz.cfg_name_ph');
  return '<div class="de-field">'
    + '<label>' + t('wiz.cfg_name_lbl') + '</label>'
    + '<input type="text" id="wiz-cfg-name" value="' + (_wiz.configName || ph) + '" '
    + 'placeholder="' + ph + '" autocomplete="off">'
    + '</div>'
    + '<div class="wiz-summary">'
    + '<p class="wiz-summary-title">' + t('wiz.summary_title') + '</p>'
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

  var titles = [t('wiz.lang_title'), t('wiz.title_board'), t('wiz.title_buses'), t('wiz.title_exp'), t('wiz.title_summary')];
  var title = step <= 4 ? titles[step] : '';
  if (total === 4 && step === 4) title = titles[4];

  var body = '';
  if (step === 0) body = _wizRenderStep0();
  else if (step === 1) body = _wizRenderStep1();
  else if (step === 2) body = _wizRenderStep2();
  else if (step === 3) body = _wizRenderStep3();
  else if (step === 4) body = _wizRenderStep4();

  var isFirst = (step === 0);
  var isLast = (step === 4);

  modal.innerHTML =
    '<div class="wizard-header">'
    + '<div class="wizard-progress-track"><div class="wizard-progress-fill" style="width:' + pct + '%"></div></div>'
    + '<div class="wizard-step-info">' + t('wiz.step_label') + ' ' + display + ' / ' + total + '</div>'
    + '<h2 class="wizard-title">' + title + '</h2>'
    + '</div>'
    + '<div class="wizard-body">' + body + '</div>'
    + '<div class="wizard-footer">'
    + '<button class="wizard-btn wizard-btn-cancel" onclick="_wizClose()">' + t('wiz.cancel') + '</button>'
    + (isFirst ? '' : '<button class="wizard-btn wizard-btn-back" onclick="wizBack()">' + t('wiz.back') + '</button>')
    + '<span style="flex:1"></span>'
    + (isLast
      ? '<button class="wizard-btn wizard-btn-finish" id="wiz-finish-btn" onclick="wizFinish()">' + t('wiz.finish') + '</button>'
      : '<button class="wizard-btn wizard-btn-next" onclick="wizNext()">' + t('wiz.next') + '</button>')
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
  if (_wiz.step === 3 && _wizTotalSteps() === 4) _wiz.step--;
  _wizRender();
}

function wizNext() {
  if (!_wiz) return;
  _wizSaveCurrent();
  _wiz.step++;
  if (_wiz.step === 3 && _wizTotalSteps() === 4) _wiz.step++;
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

  // Get default I2C address based on board type (uses i2c_known.json)
  var i2cAddr = 64;
  if (busLocalKey === 'i2c') {
    i2cAddr = _wizGetDefaultI2cAddress(type);
    // If this address is already used, try to find next available
    var usedAddrs = _wiz.expansionBoards
      .filter(function (b) { return b.busLocalKey === 'i2c' && b.i2cAddress; })
      .map(function (b) { return b.i2cAddress; });
    if (usedAddrs.indexOf(i2cAddr) !== -1) {
      // Address collision — increment until free
      while (usedAddrs.indexOf(i2cAddr) !== -1 && i2cAddr < 127) {
        i2cAddr++;
      }
    }
  }

  _wiz.expansionBoards.push({
    _idx: _wizExpIdx++, busLocalKey: busLocalKey, busKey: busKey,
    type: type, id: id, i2cAddress: i2cAddr
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
    // Update I2C address to match new board type
    if (board.busLocalKey === 'i2c') {
      var newAddr = _wizGetDefaultI2cAddress(value);
      // Check if new address conflicts with existing boards
      var usedAddrs = _wiz.expansionBoards
        .filter(function (b) { return b.busLocalKey === 'i2c' && b._idx !== idx && b.i2cAddress; })
        .map(function (b) { return b.i2cAddress; });
      if (usedAddrs.indexOf(newAddr) !== -1) {
        // Conflict — increment until free
        while (usedAddrs.indexOf(newAddr) !== -1 && newAddr < 127) {
          newAddr++;
        }
      }
      board.i2cAddress = newAddr;
    }
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

  // Compile-time buses — auto-included so a wizard config matches what the firmware
  // can do: the DCC decoder (pin from DCC_PIN) and the uart0 serial-log console.
  // Both remain removable afterwards from the Bus tab.
  var feat = (_dbgStatus && _dbgStatus.features) || {};
  var sp = (_dbgStatus && _dbgStatus.sys_pins) || {};
  if (feat.dcc) {
    var dccPin = null;
    Object.keys(sp).forEach(function (g) { if (sp[g] === 'DCC') dccPin = parseInt(g, 10); });
    // sys_pins only lists DCC while a dcc bus is active (#19) — on a fresh
    // wizard config none is active yet, so fall back to the compiled default.
    if (dccPin === null && _dbgStatus && _dbgStatus.dcc_pin_default !== undefined) {
      dccPin = parseInt(_dbgStatus.dcc_pin_default, 10);
    }
    if (dccPin !== null) cfg.buses.dcc = { type: 'dcc', pin: dccPin };
  }
  if (feat.log_serial || feat.debug_serial) {
    cfg.buses.uart0 = { type: 'uart', tx: 1, rx: 3, baud: 115200 };
  }

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

  // Validate: check for duplicate I2C addresses
  var i2cBoards = _wiz.expansionBoards.filter(function (b) { return b.busLocalKey === 'i2c'; });
  var i2cAddrs = i2cBoards.map(function (b) { return b.i2cAddress; });
  var duplicates = i2cAddrs.filter(function (addr, idx) { return i2cAddrs.indexOf(addr) !== idx; });
  if (duplicates.length > 0) {
    var errEl = document.getElementById('wiz-err');
    if (errEl) {
      errEl.textContent = t('wiz.err_duplicate_i2c') || 'Erreur : adresses I2C en double (0x'
        + duplicates[0].toString(16) + ')';
      errEl.style.display = 'block';
    }
    return;
  }

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
    : fetch('/api/board-types').then(function (r) { return r.json(); }).then(_stripMeta).catch(function () { return {}; });

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
    : fetch('/api/board-types').then(function (r) { return r.json(); }).then(_stripMeta).catch(function () { return {}; });

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
