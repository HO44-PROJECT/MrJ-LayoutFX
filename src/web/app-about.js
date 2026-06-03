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

/* ── About ──────────────────────────────────────────────────────────── */

// Zero-pad (duplicate of the one in app-core.js — kept here to avoid cross-section dependency).
function pad2(n) { return n < 10 ? '0' + n : String(n); }

var _featTipEl = null;

function showFeatTip(e, msg) {
  if (_featTipEl) { hideFeatTip(); return; }
  _featTipEl = document.createElement('div');
  _featTipEl.className = 'abt-feat-tip';
  _featTipEl.textContent = msg;
  document.body.appendChild(_featTipEl);
  var r = e.target.getBoundingClientRect();
  _featTipEl.style.left = r.left + 'px';
  _featTipEl.style.top = (r.bottom + 6) + 'px';
  setTimeout(function () { document.addEventListener('click', hideFeatTip, { once: true }); }, 0);
}

function hideFeatTip() {
  if (_featTipEl) { _featTipEl.remove(); _featTipEl = null; }
}

function fmtBytes(b) {
  if (b === undefined || b === null) return '—';
  if (b >= 1048576) return (b / 1048576).toFixed(1) + ' MB';
  if (b >= 1024) return (b / 1024).toFixed(1) + ' KB';
  return b + ' B';
}

function fmtUptime(s) {
  var h = Math.floor(s / 3600);
  var m = Math.floor((s % 3600) / 60);
  var sec = s % 60;
  return pad2(h) + ':' + pad2(m) + ':' + pad2(sec);
}

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
    var tempPct = Math.min(100, Math.max(0, Math.round((s.temp_c - 20) * 100 / 80)));
    html += abtCard(t('abt.temp'), [
      { label: 'CPU', value: s.temp_c.toFixed(1) + ' °C', bar: tempPct },
    ]);
  }

  // WiFi
  if (s.wifi_ssid !== undefined) {
    var rssi = s.wifi_rssi || 0;
    var rssiPct = Math.min(100, Math.max(0, Math.round((rssi + 100) * 2)));
    var rssiLabel = rssi >= -60 ? '🟢' : rssi >= -75 ? '🟡' : '🔴';
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

  // Features (build flags)
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
