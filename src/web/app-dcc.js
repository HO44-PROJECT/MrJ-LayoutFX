/**
 * @file app-dcc.js
 * @brief Diagnostics tab: live DCC activity indicators (GET /api/dcc-status).
 *
 * Polls /api/dcc-status while the Diagnostics tab is open and renders one
 * pill per message category (bus/speed/func/accessory/signal), lighting up
 * briefly whenever a fresh packet of that kind is seen. Lets the user tell
 * "bus dead" (nothing lights up, not even the raw pill) from "bus alive but
 * this decoder ignores it" (raw lights up, the others don't) at a glance —
 * the two PCB-vs-breadboard DCC symptoms this screen exists to distinguish.
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

var DCC_POLL_MS = 1000; // faster than the cockpit POLL: activity pills need to feel live
var DCC_LIT_MS = 1500;  // how long a pill stays "lit" after its last packet
var _dccPollTimer = null;

var DCC_KINDS = [
  { key: 'raw', i18n: 'dbg.dcc_raw', tip: 'dbg.dcc_raw_tip' },
  { key: 'speed', i18n: 'dbg.dcc_speed', tip: 'dbg.dcc_speed_tip' },
  { key: 'func', i18n: 'dbg.dcc_func', tip: 'dbg.dcc_func_tip' },
  { key: 'accessory', i18n: 'dbg.dcc_accessory', tip: 'dbg.dcc_accessory_tip' },
  { key: 'signal', i18n: 'dbg.dcc_signal', tip: 'dbg.dcc_signal_tip' }
];

// Raw bus packets are counted (pill) but never appended to the decoded-event log
// (every single packet would flood it), so there is nothing to filter for 'raw'.
var DCC_LOG_KINDS = DCC_KINDS.filter(function (k) { return k.key !== 'raw'; });
var _dccLogFilter = 'all'; // 'all' or one of DCC_LOG_KINDS[].key
var _dccLastAgeMs = 0;

// The firmware now keeps one ring buffer per category (see logDccEvent in DccDrivable.h),
// so a category's entries can no longer be evicted by another category's traffic — but the
// buffer is still small and the poll is still 1/s, so a burst between two polls can still
// scroll past. The client keeps its own capped per-category history on top, appended to
// from each poll's log[], so a filtered view never loses an entry it has already shown.
// Capped (not unbounded) to bound browser memory on a diagnostics tab left open for a
// long session.
var DCC_LOG_HISTORY_CAP = 200; // per category — generous; this is browser RAM, not the ESP32's
var _dccLogHistory = {};       // kind -> array of entries, oldest first
var _dccLogSeen = {};          // kind -> Set of "atMs:address:value" already appended, for de-dup across polls
var _dccLastRawLog = [];       // most recent full log[] from the firmware, used for the unfiltered "all" view
DCC_LOG_KINDS.forEach(function (k) {
  _dccLogHistory[k.key] = [];
  _dccLogSeen[k.key] = new Set();
});

function dccLogEntryId(e) {
  return e.last_ms + ':' + e.address + ':' + e.value;
}

// Merge freshly polled entries into the per-category history, oldest-to-newest,
// skipping any entry already recorded (repeat-count bumps re-send the same atMs
// until the count changes, so identity is last_ms+address+value, not array position).
function dccLogHistoryMerge(entries) {
  entries.forEach(function (e) {
    var hist = _dccLogHistory[e.kind];
    var seen = _dccLogSeen[e.kind];
    if (!hist || !seen) return; // unknown/raw kind — nothing to accumulate
    var id = dccLogEntryId(e);
    var last = hist[hist.length - 1];
    if (last && dccLogEntryId(last) === id) {
      last.repeat = e.repeat; // same in-place entry (repeat count ticking up) — refresh, don't duplicate
      return;
    }
    if (seen.has(id)) return;
    hist.push(e);
    seen.add(id);
    if (hist.length > DCC_LOG_HISTORY_CAP) {
      var dropped = hist.shift();
      seen.delete(dccLogEntryId(dropped));
    }
  });
}

function startDccPoll() {
  stopDccPoll();
  pollDccStatus();
  _dccPollTimer = setInterval(pollDccStatus, DCC_POLL_MS);
}

function stopDccPoll() {
  if (_dccPollTimer) {
    clearInterval(_dccPollTimer);
    _dccPollTimer = null;
  }
}

function pollDccStatus() {
  fetch('/api/dcc-status')
    .then(function (r) { return r.json(); })
    .then(renderDccStatus)
    .catch(function () { /* transient network hiccup — keep last render, try again next tick */ });
}

function renderDccStatus(d) {
  var panel = document.getElementById('dcc-panel');
  if (!d || !d.enabled) {
    panel.style.display = 'none';
    stopDccPoll();
    return;
  }
  panel.style.display = '';

  // Each pill is a filter button too — raw/"bus" has no detailed log (only ever counted,
  // never appended via logDccEvent), so it lights up but stays non-clickable, same as
  // there being no dedicated "all" pill: clicking the active category's pill again is
  // how you get back to "all".
  var row = document.getElementById('dcc-leds');
  if (!row.children.length) {
    row.innerHTML = DCC_KINDS.map(function (k) {
      var filterable = k.key !== 'raw';
      var tag = filterable ? 'button' : 'div';
      var attrs = filterable ? ' type="button" data-filter="' + k.key + '"' : '';
      return '<' + tag + ' class="dcc-led" id="dcc-led-' + k.key + '"' + attrs + '>' +
        '<span class="dcc-led-dot"></span>' +
        '<span class="dcc-led-label" data-i18n="' + k.i18n + '"></span>' +
        '<span class="dcc-led-count" id="dcc-count-' + k.key + '">0</span>' +
        '</' + tag + '>';
    }).join('');
    applyLang();
    row.querySelectorAll('.dcc-led[data-filter]').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var key = btn.getAttribute('data-filter');
        _dccLogFilter = _dccLogFilter === key ? 'all' : key;
        row.querySelectorAll('.dcc-led[data-filter]').forEach(function (b) {
          b.classList.toggle('active', b.getAttribute('data-filter') === _dccLogFilter);
        });
        renderDccLog(_dccLastAgeMs);
      });
    });
  }

  var ageMs = d.uptime_ms || 0;
  DCC_KINDS.forEach(function (k) {
    var m = (d.messages || {})[k.key] || { count: 0, last_ms: 0 };
    var el = document.getElementById('dcc-led-' + k.key);
    if (!el) return;
    var lit = m.last_ms > 0 && (ageMs - m.last_ms) <= DCC_LIT_MS;
    el.classList.toggle('lit', lit);
    document.getElementById('dcc-count-' + k.key).textContent = m.count;
    el.title = t(k.tip) + ' — ' + m.count;
  });

  var freshLog = d.log || [];
  dccLogHistoryMerge(freshLog);
  _dccLastRawLog = freshLog;
  _dccLastAgeMs = ageMs;
  renderDccLog(ageMs);
}

function dccKindLabel(kind) {
  var found = DCC_KINDS.filter(function (k) { return k.key === kind; })[0];
  return found ? t(found.i18n) : kind;
}

function dccAgoLabel(ageMs, atMs) {
  var deltaS = Math.max(0, (ageMs - atMs) / 1000);
  return '-' + deltaS.toFixed(1) + 's';
}

function renderDccLog(ageMs) {
  var log = document.getElementById('dcc-log');
  // "All" shows the raw firmware buffer as-is; a category filter shows the client-side
  // history for that category instead, so it stays FIFO-stable even after the firmware's
  // shared ring buffer has evicted the same entries under unrelated traffic.
  var visible = _dccLogFilter === 'all' ? _dccLastRawLog : (_dccLogHistory[_dccLogFilter] || []);
  if (!visible.length) {
    log.innerHTML = '<div class="dcc-log-empty" data-i18n="dbg.dcc_log_empty"></div>';
    applyLang();
    return;
  }
  // Newest first: the log array arrives oldest-to-newest from the firmware ring buffer.
  // Always emit all 6 cells (repeat empty when not applicable) so the grid's columns
  // stay aligned across every row, not just the ones with a repeat count.
  var rows = visible.slice().reverse().map(function (e) {
    var device = e.device ? e.device : '—';
    var repeat = e.repeat > 1 ? ('×' + e.repeat) : '';
    return '<div class="dcc-log-row">' +
      '<span class="dcc-log-time">' + dccAgoLabel(ageMs, e.last_ms) + '</span>' +
      '<span class="dcc-log-kind">' + dccKindLabel(e.kind) + '</span>' +
      '<span class="dcc-log-addr">#' + e.address + '</span>' +
      '<span class="dcc-log-value">' + e.value + '</span>' +
      '<span class="dcc-log-repeat">' + repeat + '</span>' +
      '<span class="dcc-log-device">' + device + '</span>' +
      '</div>';
  });
  log.innerHTML = rows.join('');
}
