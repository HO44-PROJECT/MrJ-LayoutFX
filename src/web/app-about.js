/* ── About ──────────────────────────────────────────────────────────── */

// Zero-pad (duplicate of the one in app-core.js — kept here to avoid cross-section dependency).
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
  if (b === undefined || b === null) return '—';
  if (b >= 1048576) return (b / 1048576).toFixed(1) + ' MB';
  if (b >= 1024) return (b / 1024).toFixed(1) + ' KB';
  return b + ' B';
}

// Format an uptime in seconds as "Dj HH:MM:SS" (day part omitted if d=0).
function fmtUptime(s) {
  var d = Math.floor(s / 86400);
  var h = Math.floor((s % 86400) / 3600);
  var m = Math.floor((s % 3600) / 60);
  var sec = s % 60;
  var str = pad2(h) + ':' + pad2(m) + ':' + pad2(sec);
  return d > 0 ? d + t('abt.day_unit') + ' ' + str : str;
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
    { label: t('abt.version'), value: s.version || '—' },
    { label: t('abt.build'), value: s.build_date || '—' },
    { label: t('abt.env'), value: s.env || '—' },
  ]);

  // System
  var chip = (s.chip || 'ESP32') + ' rev. ' + (s.chip_rev !== undefined ? s.chip_rev : '?');
  html += abtCard(t('abt.system'), [
    { label: t('abt.chip'), value: chip },
    { label: t('abt.cpu_freq'), value: (s.cpu_mhz || '—') + ' MHz' },
    { label: t('abt.uptime'), value: fmtUptime(s.uptime_s || 0) },
  ]);

  // Memory
  var heapPct = s.heap_total ? Math.round((1 - s.heap_free / s.heap_total) * 100) : 0;
  html += abtCard(t('abt.memory'), [
    {
      label: t('abt.heap_free'),
      value: fmtBytes(s.heap_free) + ' / ' + fmtBytes(s.heap_total),
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
      value: fmtBytes(fwUsed) + ' / ' + fmtBytes(fwTotal),
      bar: fwPct
    },
  ]);

  // Filesystem
  if (s.fs_total !== undefined) {
    var fsPct = s.fs_total ? Math.round(s.fs_used / s.fs_total * 100) : 0;
    html += abtCard(t('abt.fs'), [
      {
        label: 'LittleFS',
        value: fmtBytes(s.fs_used) + ' / ' + fmtBytes(s.fs_total),
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
        value: s.devices + ' / ' + devMax,
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
        value: s.temp_c.toFixed(1) + ' °C',
        bar: tempPct
      },
    ]);
  }

  // WiFi
  if (s.wifi_ssid !== undefined) {
    var rssi = s.wifi_rssi || 0;
    var rssiPct = Math.min(100, Math.max(0, Math.round((rssi + 100) * 2)));
    var rssiLabel = rssi >= -60 ? '🟢' : rssi >= -75 ? '🟡' : '🔴';
    html += abtCard(t('abt.wifi'), [
      { label: t('abt.wifi_ssid'), value: s.wifi_ssid || '—' },
      { label: t('abt.wifi_rssi'), value: rssiLabel + ' ' + rssi + ' dBm', bar: rssiPct },
      { label: t('abt.wifi_mac'), value: s.wifi_mac || '—' },
    ]);
  }

  // Libraries
  if (s.libs) {
    var libRows = Object.keys(s.libs).map(function (k) {
      return { label: k, value: s.libs[k] || '—' };
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
