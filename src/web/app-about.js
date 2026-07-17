/**
 * @file app-about.js
 * @brief About panel with system information and project links.
 *
 * Displays firmware version, hardware info, memory usage, and provides
 * links to project resources (GitHub, wiki, issues).
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

var PROJECT_URLS = {
  git:    'https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX',
  issues: 'https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX/issues',
  wiki:   '' // placeholder — to be filled when wiki is published
};

// Status bar warn/crit color thresholds (percent), shared by every metric
// bar in this panel (heap, flash, devices, temp, RSSI...). See abtCard.
var BAR_PCT_WARN = 65;
var BAR_PCT_CRIT = 85;

// WiFi RSSI bands (dBm), typical for ESP32/IoT devices: >=GOOD is a strong
// link, between WEAK and GOOD is usable but marginal, below WEAK is poor.
// Named thresholds so the emoji label and the status bar's warn/crit
// coloring (BAR_PCT_WARN/CRIT above) can never drift apart.
var RSSI_DBM_GOOD = -70;
var RSSI_DBM_WEAK = -80;

// CPU temperature bar range (°C): the bar is 0% at MIN and 100% at MAX —
// arbitrary display bounds (ESP32 chips run warm at idle; not a datasheet limit).
var TEMP_C_BAR_MIN = 20;
var TEMP_C_BAR_MAX = 100;

// heapPct / fsPct / devPct / fwPct below are plain used/total ratios (0-100%
// falls out of the math directly) — no arbitrary bound to name there.

/* ── About ──────────────────────────────────────────────────────────── */

// Zero-pad (duplicate of the one in app-core.js — kept here to avoid cross-section dependency).
function pad2(n) { return n < 10 ? '0' + n : String(n); }


// ── OTA firmware upload (inline in the About card) ───────────────────────────
// Reflects the chosen file name and enables the send button.
function otaPick() {
  var f = document.getElementById('ota-file').files[0];
  document.getElementById('ota-name').textContent = f ? f.name : t('abt.ota_nofile');
  document.getElementById('ota-send').disabled = !f;
}

// Streams the selected .bin to POST /update with a progress bar, then reboots.
function otaUpload() {
  var f = document.getElementById('ota-file').files[0];
  if (!f) return;
  var prog = document.getElementById('ota-prog');
  var status = document.getElementById('ota-status');
  var send = document.getElementById('ota-send');
  var fd = new FormData();
  fd.append('firmware', f);
  var x = new XMLHttpRequest();
  send.disabled = true;
  prog.hidden = false;
  prog.value = 0;
  status.textContent = '';
  x.upload.onprogress = function (ev) {
    if (ev.lengthComputable) prog.value = ev.loaded / ev.total * 100;
  };
  x.onload = function () {
    status.textContent = x.responseText;
    if (x.status === 200) setTimeout(function () { location = '/ui'; }, 7000);
    else send.disabled = false;
  };
  x.onerror = function () { status.textContent = t('abt.ota_neterr'); send.disabled = false; };
  x.open('POST', '/update');
  x.send(fd);
}

function fmtBytes(b) {
  if (b === undefined || b === null) return '—';
  if (b >= 1073741824) return (b / 1073741824).toFixed(1) + ' GB';
  if (b >= 1048576) return (b / 1048576).toFixed(1) + ' MB';
  if (b >= 1024) return (b / 1024).toFixed(1) + ' KB';
  return b + ' B';
}

// uptime_s comes from millis()/1000UL, a uint32_t that wraps at ~49.7 days —
// a known Arduino-core limitation, not handled here (DCC layouts reboot far
// more often than that in practice).
function fmtUptime(s) {
  var d = Math.floor(s / 86400);
  var h = Math.floor((s % 86400) / 3600);
  var m = Math.floor((s % 3600) / 60);
  var sec = s % 60;
  var hms = pad2(h) + ':' + pad2(m) + ':' + pad2(sec);
  return d > 0 ? d + t('abt.uptime_day') + ' ' + hms : hms;
}

function abtCard(title, rows) {
  return '<div class="abt-card">'
    + '<div class="abt-card-title">' + title + '</div>'
    + rows.map(function (r) {
      var barHtml = '';
      if (r.bar !== undefined) {
        var cls = r.bar > BAR_PCT_CRIT ? ' crit' : r.bar > BAR_PCT_WARN ? ' warn' : '';
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
// Three sections: runtime (live values), build (compile-time), project (structural/links).
function renderAbout(s) {
  var html = '';

  // ── Instantané ─────────────────────────────────────────────────────────────
  html += '<div class="abt-section-hdr">' + t('abt.sec_runtime') + '</div>';

  // System (chip, CPU, uptime)
  var chip = (s.chip || 'ESP32') + ' rev. ' + (s.chip_rev !== undefined ? s.chip_rev : '?');
  html += abtCard(t('abt.system'), [
    { label: t('abt.chip'), value: chip },
    { label: t('abt.cpu_freq'), value: (s.cpu_mhz || '—') + ' MHz' },
    { label: t('abt.uptime'), value: fmtUptime(s.uptime_s || 0) },
  ]);

  // Memory
  var heapPct = s.heap_total ? Math.round((1 - s.heap_free / s.heap_total) * 100) : 0;
  html += abtCard(t('abt.memory'), [
    { label: t('abt.heap_free'), value: fmtBytes(s.heap_free) + ' / ' + fmtBytes(s.heap_total), bar: heapPct },
    { label: t('abt.heap_min'), value: fmtBytes(s.heap_min) },
  ]);

  // Filesystem
  if (s.fs_total !== undefined) {
    var fsPct = s.fs_total ? Math.round(s.fs_used / s.fs_total * 100) : 0;
    html += abtCard(t('abt.fs'), [
      { label: 'LittleFS', value: fmtBytes(s.fs_used) + ' / ' + fmtBytes(s.fs_total), bar: fsPct },
    ]);
  }

  // Config / devices
  if (s.devices !== undefined) {
    var devMax = s.devices_max || 0;
    var devPct = devMax ? Math.round(s.devices / devMax * 100) : 0;
    html += abtCard(t('abt.config'), [
      { label: t('abt.devices'), value: s.devices + ' / ' + devMax, bar: devPct },
    ]);
  }

  // Temperature
  if (s.temp_c !== undefined) {
    var tempPct = Math.min(100, Math.max(0, Math.round(
      (s.temp_c - TEMP_C_BAR_MIN) * 100 / (TEMP_C_BAR_MAX - TEMP_C_BAR_MIN))));
    html += abtCard(t('abt.temp'), [
      { label: 'CPU', value: s.temp_c.toFixed(1) + ' °C / ' + (s.temp_c * 9 / 5 + 32).toFixed(1) + ' °F', bar: tempPct },
    ]);
  }

  // WiFi
  if (s.wifi_ssid !== undefined) {
    var rssi = s.wifi_rssi || 0;
    // Bar reflects signal weakness (like the other cards: high bar = worse),
    // so a strong signal (rssi close to 0) yields a low/green bar. Linear fit
    // through (RSSI_DBM_GOOD -> BAR_PCT_WARN) and (RSSI_DBM_WEAK -> BAR_PCT_CRIT)
    // so the bar's warn/crit coloring always lines up with the RSSI bands below.
    var rssiSlope = (BAR_PCT_CRIT - BAR_PCT_WARN) / (RSSI_DBM_WEAK - RSSI_DBM_GOOD);
    var rssiPct = Math.min(100, Math.max(0, Math.round(
      BAR_PCT_WARN + (rssi - RSSI_DBM_GOOD) * rssiSlope)));
    var rssiLabel = rssi >= RSSI_DBM_GOOD ? '🟢' : rssi >= RSSI_DBM_WEAK ? '🟡' : '🔴';
    html += abtCard(t('abt.wifi'), [
      { label: t('abt.wifi_ssid'), value: s.wifi_ssid || '—' },
      { label: t('abt.wifi_rssi'), value: rssiLabel + ' ' + rssi + ' dBm', bar: rssiPct },
      { label: t('abt.wifi_mac'), value: s.wifi_mac || '—' },
    ]);
  }

  // ── Compilation ────────────────────────────────────────────────────────────
  html += '<div class="abt-section-hdr">' + t('abt.sec_build') + '</div>';

  // Firmware (version, build date, env)
  html += abtCard(t('abt.firmware'), [
    { label: t('abt.version'), value: s.version || '—' },
    { label: t('abt.build'), value: s.build_date || '—' },
    { label: t('abt.env'), value: s.env || '—' },
  ]);

  // Flash (binary size)
  var fwUsed = s.sketch_size || 0;
  var fwTotal = fwUsed + (s.sketch_free || 0);
  var fwPct = fwTotal ? Math.round(fwUsed / fwTotal * 100) : 0;
  html += abtCard(t('abt.flash'), [
    { label: t('abt.firmware_size'), value: fmtBytes(fwUsed) + ' / ' + fmtBytes(fwTotal), bar: fwPct },
  ]);

  // Firmware update over-the-air — inline uploader, only when compiled in (#define OTA)
  if (s.features && s.features.ota) {
    html += '<div class="abt-card"><div class="abt-card-title">' + t('abt.ota') + '</div>'
      + '<div style="font-size:.72rem;color:var(--t2);margin:.1rem 0 .6rem">' + t('abt.ota_hint') + '</div>'
      + '<input type="file" id="ota-file" accept=".bin" hidden onchange="otaPick()">'
      + '<label class="abt-refresh" for="ota-file" style="display:inline-block">' + t('abt.ota_choose') + '</label>'
      + ' <span id="ota-name" style="font-size:.72rem;color:var(--t2)">' + t('abt.ota_nofile') + '</span>'
      + '<div style="margin-top:.6rem"><button class="abt-refresh" id="ota-send" onclick="otaUpload()" disabled>' + t('abt.ota_btn') + '</button></div>'
      + '<progress id="ota-prog" value="0" max="100" hidden style="width:100%;margin-top:.6rem"></progress>'
      + '<div id="ota-status" style="font-size:.72rem;margin-top:.4rem;white-space:pre-wrap"></div>'
      + '</div>';
  }

  // Features (build flags)
  if (s.features) {
    var FEAT_LABELS = {
      config: 'Config', api: 'API', api_audit: 'API audit', webui: 'WebUI', wifi: 'WiFi', wifi_force_ap: 'Force AP', ota: 'OTA',
      dcc: 'DCC', dcc_audit: 'DCC audit',
      spi: 'SPI', i2c: 'I²C', i2c_scan: 'I²C scan',
      lobot_servo: 'Lobot Servo', lx16a_servo: 'LX-16A Servo',
      audio: 'Audio',
      oled: 'OLED', oled_status: 'OLED status', oled_splash: 'OLED splash',
      oled_metrics: 'OLED metrics', oled_events: 'OLED events',
      log_serial: 'Serial log', debug_serial: 'Serial debug', log_oled: 'OLED log', debug_oled: 'OLED debug',
      jtag: 'JTAG', demo: 'Demo'
    };
    // Grouped by category (mirrors MrJRailwayFX_define.h) — one labelled row each,
    // so 26 badges read as 5 tidy lines instead of one blob.
    var FEAT_GROUPS = [
      ['abt.featgrp.core',   ['config', 'api', 'api_audit', 'webui', 'wifi', 'wifi_force_ap', 'ota']],
      ['abt.featgrp.bus',    ['dcc', 'dcc_audit', 'spi', 'i2c', 'i2c_scan', 'lobot_servo', 'lx16a_servo', 'audio']],
      ['abt.featgrp.oled',   ['oled', 'oled_status', 'oled_splash', 'oled_metrics', 'oled_events']],
      ['abt.featgrp.log',    ['log_serial', 'debug_serial', 'log_oled', 'debug_oled']],
      ['abt.featgrp.behave', ['jtag', 'demo']]
    ];
    var groups = FEAT_GROUPS.map(function (g) {
      var keys = g[1].filter(function (k) { return s.features[k] !== undefined; });
      if (!keys.length) return '';
      var badges = keys.map(function (k) {
        var tip = t('abt.feat.' + k).replace(/&/g, '&amp;').replace(/"/g, '&quot;');
        return '<span class="abt-feat' + (s.features[k] ? ' on' : ' off') + '" data-tip="' + tip + '">' + FEAT_LABELS[k] + '</span>';
      }).join('');
      return '<div class="abt-feat-grp"><span class="abt-feat-grp-lbl">' + t(g[0]) + '</span>'
        + '<div class="abt-feat-row">' + badges + '</div></div>';
    }).join('');
    html += '<div class="abt-card abt-feat-card"><div class="abt-card-title">' + t('abt.features') + '</div>'
      + groups + '</div>';
  }

  // Libraries
  if (s.libs) {
    var libRows = Object.keys(s.libs).map(function (k) {
      return { label: k, value: s.libs[k] || '—' };
    });
    html += abtCard(t('abt.libs'), libRows);
  }

  // ── Projet ─────────────────────────────────────────────────────────────────
  html += '<div class="abt-section-hdr">' + t('abt.sec_project') + '</div>';

  // Project links
  var urlLinks = [
    { label: t('abt.proj_git'),    url: PROJECT_URLS.git },
    { label: t('abt.proj_issues'), url: PROJECT_URLS.issues },
    { label: t('abt.proj_wiki'),   url: PROJECT_URLS.wiki },
  ].filter(function (r) { return r.url; });

  if (urlLinks.length) {
    var linkRows = urlLinks.map(function (r) {
      return '<div class="abt-url-row">'
        + '<span class="abt-url-label">' + r.label + '</span>'
        + '<a class="abt-url-val" href="' + r.url + '" target="_blank" rel="noopener">'
        + r.url.replace(/^https?:\/\//, '') + '</a>'
        + '</div>';
    }).join('');
    html += '<div class="abt-card"><div class="abt-card-title">' + t('abt.project') + '</div>'
      + linkRows + '</div>';
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
}
