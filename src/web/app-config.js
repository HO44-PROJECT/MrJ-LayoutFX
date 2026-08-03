/**
 * @file app-config.js
 * @brief Configuration management and device state actions.
 *
 * Handles config file upload/download, device state management from cockpit,
 * and debug data loading for the WebUI.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

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

// Turn all devices sharing a given DCC address on or off (#10).
function groupByAddr(address, state) {
  post('/api/group', { address: address, state: state }).then(poll).catch(showErr);
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
          + '<span class="cfg-fname" title="' + f + '">' + f + '</span>'
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
    var parsed;
    try { parsed = JSON.parse(e.target.result); }
    catch (err) { cfgStatus('JSON invalide : ' + err.message, 'err'); return; }

    // validateConfig is generated at build time from schemas/config.schema.json
    // (see gen_config_validator.py) and absent when Node/Ajv weren't available
    // at build time, or NO_CONFIG_SCHEMA_VALIDATION was set — in which case
    // upload falls back to the JSON.parse() check above only.
    var schemaValidated = typeof validateConfig === 'function';
    if (schemaValidated && !validateConfig(parsed)) {
      cfgErrorList(validateConfig.errors || []);
      return;
    }
    cfgErrorList([]); // clear any previous validation error list

    document.getElementById('cfg-upload-btn').disabled = true;
    cfgStatus('Envoi en cours… → ' + safeName, 'ok');

    fetch('/api/configs?name=' + encodeURIComponent(safeName), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Config-Name': safeName },
      body: e.target.result
    })
      .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
      .then(function () {
        cfgStatus(t(schemaValidated ? 'cfg.saved_validated' : 'cfg.saved'), 'ok');
        document.getElementById('cfg-upload-btn').disabled = false;
        loadConfigs();
      })
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

// Render Ajv errors (from validateConfig.errors, see uploadConfig()) as a
// short, readable, translated list — instead of dumping Ajv's raw messages
// into the single-line .cfg-status banner (unreadable wall of bold text).
// Pass an empty array to hide/clear the list.
function cfgErrorList(errors) {
  var box = document.getElementById('cfg-err-list');
  if (!errors.length) { box.style.display = 'none'; box.innerHTML = ''; return; }

  // Dedup + i18n templating is pure logic, unit-tested in app-pure.js —
  // this function only turns the resulting lines into DOM.
  var lines = formatConfigErrors(errors, t);

  var title = t('cfg.err.title', { n: lines.length });
  box.innerHTML = '<div class="cfg-err-list-title">' + escapeHtml(title) + '</div><ul>' +
    lines.map(function (l) { return '<li>' + escapeHtml(l) + '</li>'; }).join('') + '</ul>';
  box.style.display = '';
}

/* ── WiFi provisioning gate (#132) ─────────────────────────────────────
   Shown on top of the normal UI when /api/status reports wifiMode:'ap' —
   the device isn't joined to a home network yet. Saving POSTs to
   /api/wifi, which persists the credentials to LittleFS and reboots the
   device into STA (falling back to AP again if they don't work, same
   logic as ApiServer::init()). The device then comes up on a DIFFERENT
   IP (the home network's, not 192.168.4.1) — unlike the config-reload
   reboot flow (tryReconnect()), this page cannot auto-follow that, so we
   just tell the user to reconnect their own device to their WiFi and
   navigate to the new IP themselves. */

function wifiGateShow() {
  document.getElementById('wifi-gate-overlay').classList.add('active');
  document.getElementById('wifi-gate-modal').classList.add('active');
  wifiGateScan();
}

// Populate a custom clickable results list under the SSID field with nearby
// networks — a native <datalist> doesn't reliably open on mobile (iOS Safari,
// most Android browsers require typing before it shows), and this is the
// dominant use case (phone connected to the device's AP). Clicking a row
// fills the field; typing your own SSID (hidden network) still works,
// independent of the results list. Silent on failure/empty scan.
function wifiGateScan() {
  var results = document.getElementById('wifi-ssid-results');
  results.innerHTML = '';
  fetch('/api/wifi/scan')
    .then(function (r) { return r.json(); })
    .then(function (data) {
      (data.networks || []).forEach(function (net) {
        var row = document.createElement('div');
        row.className = 'wifi-ssid-row';
        row.textContent = net.ssid;
        row.onclick = function () {
          document.getElementById('wifi-ssid').value = net.ssid;
          results.innerHTML = '';
        };
        results.appendChild(row);
      });
    })
    .catch(function () { });
}

function wifiGateHide() {
  document.getElementById('wifi-gate-overlay').classList.remove('active');
  document.getElementById('wifi-gate-modal').classList.remove('active');
}

// "Continue without WiFi" — dismiss for this browser session only (AP mode
// is inherently transient; don't remember this across a later real reboot).
function wifiGateSkip() {
  sessionStorage.setItem('mrjfx_wifi_gate_dismissed', '1');
  wifiGateHide();
}

function wifiGateStatus(msg, cls) {
  var el = document.getElementById('wifi-gate-status');
  el.style.display = msg ? '' : 'none';
  el.className = 'de-status ' + cls;
  el.textContent = msg;
}

function wifiGateSave() {
  var ssid = document.getElementById('wifi-ssid').value.trim();
  var password = document.getElementById('wifi-password').value;
  if (!ssid) { wifiGateStatus(t('wifi.err_ssid'), 'err'); return; }
  wifiGateStatus(t('wifi.saving'), 'ok');
  post('/api/wifi', { ssid: ssid, password: password })
    .then(function (r) {
      if (!r.ok) return r.json().then(function (e) { throw new Error(e.error || ('HTTP ' + r.status)); });
      // The device only ever acks "starting the test" here — it then drops
      // its own AP to attempt the target network for real (ESP32 has one
      // radio, can't hold both), so this phone is about to lose its
      // connection regardless of whether the credentials turn out to be
      // right or wrong. There is no second response coming.
      wifiGateStatus(t('wifi.testing'), 'ok');
    })
    .catch(function (e) {
      // Most likely: the request never reached the device (typo'd form
      // submitted before JS even ran, device already rebooted, etc.) — but
      // it could also mean the device accepted it and is already dropping
      // the AP. Either way, tell the user what to check next.
      wifiGateStatus(t('wifi.err_save') + e.message + ' ' + t('wifi.maybe_saved'), 'err');
    });
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
var _dbgIdentify = null;  // key of the pin currently being identified ('g17' | 'c1_p9'), or null
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
  AUDIO_TYPES = Object.keys(dt).filter(function (k) { return dt[k].category === 'audio'; });
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
      if (bus.tx >= 0) _dbgSysPins[bus.tx] = shortName + '·TX';
      if (bus.rx >= 0) _dbgSysPins[bus.rx] = shortName + '·RX';
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
  // Returned so mutating actions can chain work on FRESH data (deleteBus & co —
  // a synchronous render right after calling loadDebug() paints the stale
  // _dbgCfg while the fetches are still in flight).
  return Promise.all([pDevs, pTypes, pBoards, pCfg, pStatus, pDevTypes, pBusTypes, pI2cKnown]).then(function () {
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
