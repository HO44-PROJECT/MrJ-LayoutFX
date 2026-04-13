
    /* ── Constants ──────────────────────────────────────────────────────── */
    var POLL = 3000;
    var STATIC_TYPES = ['StaticLow'];
    var TRAFFIC_TYPES = ['TrafficLight3ph', 'TrafficLight4ph'];
    var SERVO_TYPES = ['SerialServo'];
    // Servo presets: v = speed sent to /api/servo (-1000…+1000), l = label, c = CSS class
    var SERVO_STATES = [
      { v:    0,    l: 'STOP', c: 't-stop' },
      { v:  300,    l: 'SLOW', c: 't-slow' },
      { v:  600,    l: 'MID',  c: 't-go'   },
      { v: 1000,    l: 'FAST', c: 't-flash'},
      { v: 'REV',   l: 'REV',  c: 't-sh1'  }
    ];
    // Signal state definitions: v = state int sent to /api/device, l = label, c = CSS class
    var SIGNAL_STATES = {
      'DBBlocSignal': [{ v: 0, l: 'OFF', c: 't-off' }, { v: 1, l: 'HP0', c: 't-stop' }, { v: 2, l: 'HP1', c: 't-go' }],
      'DBEntrySignal': [{ v: 0, l: 'OFF', c: 't-off' }, { v: 1, l: 'HP0', c: 't-stop' }, { v: 2, l: 'HP1', c: 't-go' }, { v: 3, l: 'HP2', c: 't-slow' }],
      'DBExitSignal': [{ v: 0, l: 'OFF', c: 't-off' }, { v: 1, l: 'HP00', c: 't-stop' }, { v: 2, l: 'HP1', c: 't-go' }, { v: 3, l: 'HP2', c: 't-slow' }, { v: 4, l: 'HP0+Sh1', c: 't-sh1' }]
    };

    /* ── Navigation ─────────────────────────────────────────────────────── */

    var _currentView = 'cockpit';

    function toggleDrawer() {
      document.body.classList.toggle('drawer-open');
    }

    function closeDrawer() {
      document.body.classList.remove('drawer-open');
    }

    function switchView(name) {
      // Hide all views
      document.querySelectorAll('.view').forEach(function (v) {
        v.classList.remove('active');
      });
      // Show target view
      document.getElementById('view-' + name).classList.add('active');

      // Update nav items
      document.querySelectorAll('.nav-item').forEach(function (item) {
        item.classList.toggle('active', item.dataset.view === name);
      });

      // Show global actions + status bar only on cockpit
      var isCockpit = name === 'cockpit';
      document.getElementById('hdr-actions').style.display = isCockpit ? '' : 'none';
      document.getElementById('sb').style.display = isCockpit ? '' : 'none';

      _currentView = name;
      location.hash = name;
      closeDrawer();
      if (name === 'params') loadParams();
      if (name === 'about') loadAbout();
      if (name === 'config') { loadDebug(); loadConfigs(); cfgStatus('', ''); renderDirtyBanner(); }
    }


    /* ── Cockpit helpers ────────────────────────────────────────────────── */
    function cls(d) {
      if (STATIC_TYPES.indexOf(d.type) >= 0) return 'static';
      if (d.state < 0) return 'busy';
      if (d.desired > 0) return 'on';
      return 'off';
    }

    function clsTraffic(d) {
      if (d.state < 0) return 'busy';
      if (d.desired === 2) return 'on';
      if (d.desired === 1) return 'stop';
      if (d.desired === 3) return 'flash';
      return 'off';
    }

    function lbl(c) {
      if (c === 'static') return t('ck.static');
      if (c === 'busy') return t('ck.busy');
      if (c === 'on') return t('ck.on');
      return t('ck.off');
    }

    function meta(d) {
      var t = '<div class="meta">';
      if (d.addr > 0) t += '<span class="mtag">DCC ' + d.addr + '</span>';
      var pinStr = (d.pins && d.pins.length > 0) ? d.pins.join(',') : null;
      if (d.board > 0) t += '<span class="mtag">Carte ' + d.board + ' · pin ' + (pinStr || '?') + '</span>';
      else if (pinStr) t += '<span class="mtag">GPIO ' + pinStr + '</span>';
      return t + '</div>';
    }

    function cardTraffic(d) {
      var c = clsTraffic(d);
      var busy = d.state < 0;
      var dis = busy ? 'disabled' : '';
      var ico = ICONS[d.type] || ICONS['_'];
      var tip = tooltip(d.type);
      function tbtn(label, cls, st) {
        var act = (d.desired === st && !busy) ? 'active' : '';
        return '<button class="tbtn t-' + cls + ' ' + act + '" onclick="setTraffic(\'' + d.id + '\',' + st + ')" ' + dis + '>' + label + '</button>';
      }
      return '<div class="card ' + c + '">'
        + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
        + '<span class="dot ' + c + '"></span></div>'
        + '<div class="icon" title="' + tip + '">' + ico + '</div>'
        + '<span class="badge">' + d.type + '</span>'
        + meta(d)
        + '<div class="tbtns">' + tbtn('OFF', 'off', 0) + tbtn('GO', 'go', 2) + tbtn('FLASH', 'flash', 3) + tbtn('STOP', 'stop', 1) + '</div>'
        + '</div>';
    }

    function cardSignal(d) {
      var states = SIGNAL_STATES[d.type];
      var c = d.state < 0 ? 'busy' : (d.desired > 0 ? 'on' : 'off');
      var busy = d.state < 0;
      var dis = busy ? 'disabled' : '';
      var ico = ICONS[d.type] || ICONS['_'];
      var tip = tooltip(d.type);
      function sbtn(s) {
        var act = (d.desired === s.v && !busy) ? 'active' : '';
        return '<button class="tbtn ' + s.c + ' ' + act + '" onclick="setSig(\'' + d.id + '\',' + s.v + ')" ' + dis + '>' + s.l + '</button>';
      }
      return '<div class="card ' + c + '">'
        + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
        + '<span class="dot ' + c + '"></span></div>'
        + '<div class="icon" title="' + tip + '">' + ico + '</div>'
        + '<span class="badge">' + d.type + '</span>'
        + meta(d)
        + '<div class="tbtns">' + states.map(sbtn).join('') + '</div>'
        + '</div>';
    }

    function cardServo(d) {
      var busy = d.state < 0;
      var dis = busy ? 'disabled' : '';
      var ico = ICONS[d.type] || ICONS['_'];
      var tip = tooltip(d.type);
      var c = busy ? 'busy' : (d.desired > 0 ? 'on' : 'off');
      // active = STOP button when off, no active highlight for speed buttons (speed not in /api/devices response)
      function sbtn(s) {
        var act = (s.v === 0 && d.desired === 0 && !busy) ? 'active' : '';
        var onclick = s.v === 'REV'
          ? 'revServo(\'' + d.id + '\')'
          : 'setServo(\'' + d.id + '\',' + s.v + ')';
        return '<button class="tbtn ' + s.c + ' ' + act + '" onclick="' + onclick + '" ' + dis + '>' + s.l + '</button>';
      }
      return '<div class="card ' + c + '">'
        + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
        + '<span class="dot ' + c + '"></span></div>'
        + '<div class="icon" title="' + tip + '">' + ico + '</div>'
        + '<span class="badge">' + d.type + '</span>'
        + meta(d)
        + '<div class="tbtns">' + SERVO_STATES.map(sbtn).join('') + '</div>'
        + '</div>';
    }

    function card(d) {
      if (TRAFFIC_TYPES.indexOf(d.type) >= 0) return cardTraffic(d);
      if (SERVO_TYPES.indexOf(d.type) >= 0) return cardServo(d);
      if (SIGNAL_STATES[d.type]) return cardSignal(d);
      var c = cls(d);
      var dis = (c === 'busy' || c === 'static') ? 'disabled' : '';
      var ico = ICONS[d.type] || ICONS['_'];
      var tip = tooltip(d.type);
      return '<div class="card ' + c + '">'
        + '<div class="ch"><span class="cid" title="' + d.id + '">' + d.id + '</span>'
        + '<span class="dot ' + c + '"></span></div>'
        + '<div class="icon" title="' + tip + '">' + ico + '</div>'
        + '<span class="badge">' + d.type + '</span>'
        + meta(d)
        + '<button class="btn ' + c + '" onclick="tog(\'' + d.id + '\',' + d.desired + ')" ' + dis + '>' + lbl(c) + '</button>'
        + '</div>';
    }

    function render(devs) {
      var order = [];
      var groups = {};
      devs.forEach(function (d) {
        if (!groups[d.type]) { groups[d.type] = []; order.push(d.type); }
        groups[d.type].push(d);
      });
      var html = '';
      order.forEach(function (type) {
        var list = groups[type];
        var isStatic = STATIC_TYPES.indexOf(type) >= 0;
        html += '<div class="group">';
        html += '<div class="ghdr"><span class="gname">' + type + ' <span class="gcnt">(' + list.length + ')</span></span>';
        if (!isStatic) {
          html += '<div class="gbtns">'
            + '<button class="gbtn on" onclick="groupDevices(\'' + type + '\',1)">' + t('ck.grp_on') + '</button>'
            + '<button class="gbtn off" onclick="groupDevices(\'' + type + '\',0)">' + t('ck.grp_off') + '</button>'
            + '</div>';
        }
        html += '</div>';
        html += '<div class="gcards">' + list.map(card).join('') + '</div>';
        html += '</div>';
      });
      document.getElementById('grid').innerHTML = html;
      var now = new Date().toLocaleTimeString('fr-FR');
      document.getElementById('sb').innerHTML = '<span>' + devs.length + '</span> appareils &mdash; ' + now;
    }

    /* ── API calls ──────────────────────────────────────────────────────── */
    function post(url, body) {
      return fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
    }

    function deleteJson(url, body) {
      return fetch(url, { method: 'DELETE', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); });
    }

    function pad2(n) { return n < 10 ? '0' + n : '' + n; }

    function showErr() { document.getElementById('err').style.display = 'block'; }

    function poll() {
      fetch('/api/devices')
        .then(function (r) { if (!r.ok) throw r; return r.json(); })
        .then(function (d) {
          document.getElementById('err').style.display = 'none';
          render(d);
          if (_currentView === 'config') { _dbgDevs = d; renderDebugBoards(); }
        })
        .catch(showErr);
    }

    function tog(id, desired) {
      post('/api/device', { id: id, state: desired > 0 ? 0 : 1 }).then(poll).catch(showErr);
    }

    function setTraffic(id, state) {
      post('/api/device', { id: id, state: state }).then(poll).catch(showErr);
    }

    function setSig(id, state) {
      post('/api/device', { id: id, state: state }).then(poll).catch(showErr);
    }

    function setServo(id, speed) {
      post('/api/servo', { id: id, speed: speed }).then(poll).catch(showErr);
    }

    function revServo(id) {
      post('/api/servo', { id: id, action: 'reverse' }).then(poll).catch(showErr);
    }

    function allDevices(state) {
      post('/api/all', { state: state }).then(poll).catch(showErr);
    }

    function groupDevices(type, state) {
      post('/api/group', { type: type, state: state }).then(poll).catch(showErr);
    }

    /* ── Config file switcher ───────────────────────────────────────────── */

    var CFG_PENDING_KEY = 'mrjfx_pending_cfg'; // persisted: file to activate on next restart
    var _cfgRenaming = null;                   // filename currently being renamed inline
    var _cfgActive   = 'config.json';          // current active source filename (from API)

    function _cfgTs() {
      var now = new Date();
      return now.getFullYear() + pad2(now.getMonth() + 1) + pad2(now.getDate())
           + '_' + pad2(now.getHours()) + pad2(now.getMinutes());
    }

    function loadConfigs() {
      fetch('/api/configs')
        .then(function (r) { return r.json(); })
        .then(function (data) {
          _cfgActive = data.active || 'config.json';
          var pending = localStorage.getItem(CFG_PENDING_KEY);
          var el = document.getElementById('cfg-filelist');
          el.innerHTML = data.files.map(function (f) {
            var isActive  = (f === data.active);
            var isPending = (!isActive && f === pending);
            var cls = 'cfg-fileitem' + (isActive ? ' active' : '') + (isPending ? ' pending' : '');
            var dot  = '<span class="cfg-filedot"></span>';
            var sf   = f.replace(/\\/g, '\\\\').replace(/'/g, "\\'");

            // Inline rename mode
            if (f === _cfgRenaming) {
              return '<div class="' + cls + '">' + dot
                + '<input id="cfg-rename-inp" class="cfg-rename-inp" type="text" maxlength="27"'
                + ' value="' + f.replace(/\.json$/, '') + '"'
                + ' onkeydown="if(event.key===\'Enter\')cfgRenameConfirm(\'' + sf + '\');'
                +             'if(event.key===\'Escape\')cfgRenameCancel()">'
                + '<button class="cfg-row-btn ok" onclick="cfgRenameConfirm(\'' + sf + '\')" title="OK">✓</button>'
                + '<button class="cfg-row-btn" onclick="cfgRenameCancel()" title="Annuler">✕</button>'
                + '</div>';
            }

            // Slot 1 : badge (active/pending) ou bouton Activer — largeur fixe via CSS
            // Slots 2-5 : Backup, Download, [Rename, Delete] ou fantômes invisibles
            // Les fantômes portent le même texte que les vrais boutons (visibility:hidden)
            // → même largeur → zone nom identique pour toutes les lignes
            var ghostRename = '<span class="cfg-row-btn cfg-row-ghost" aria-hidden="true">' + t('cfg.rename.btn') + '</span>';
            var ghostDelete = '<span class="cfg-row-btn cfg-row-ghost" aria-hidden="true">✕ ' + t('cfg.destroy.btn') + '</span>';
            var btns = '<div class="cfg-row-btns">';
            if (isActive) {
              btns += '<span class="cfg-filebadge">' + t('cfg.badge.active') + '</span>';
              btns += '<button class="cfg-row-btn" onclick="cfgSnapshot(\'config.json\')">' + t('cfg.snapshot.btn') + '</button>';
              btns += '<button class="cfg-row-btn" onclick="cfgDownload(\'config.json\',_cfgActive)">⬇ ' + t('cfg.dl.btn') + '</button>';
              btns += ghostRename + ghostDelete;
            } else if (isPending) {
              btns += '<span class="cfg-filebadge pending">' + t('cfg.badge.pending') + '</span>';
              btns += '<button class="cfg-row-btn" onclick="cfgSnapshot(\'' + sf + '\')">' + t('cfg.snapshot.btn') + '</button>';
              btns += '<button class="cfg-row-btn" onclick="cfgDownload(\'' + sf + '\')">⬇ ' + t('cfg.dl.btn') + '</button>';
              btns += '<button class="cfg-row-btn" onclick="cfgRename(\'' + sf + '\')">' + t('cfg.rename.btn') + '</button>';
              btns += '<button class="cfg-row-btn danger" onclick="cfgDelete(\'' + sf + '\')">✕ ' + t('cfg.destroy.btn') + '</button>';
            } else {
              btns += '<button class="cfg-row-btn primary" onclick="cfgChoose(\'' + sf + '\')">' + t('cfg.choose_btn') + '</button>';
              btns += '<button class="cfg-row-btn" onclick="cfgSnapshot(\'' + sf + '\')">' + t('cfg.snapshot.btn') + '</button>';
              btns += '<button class="cfg-row-btn" onclick="cfgDownload(\'' + sf + '\')">⬇ ' + t('cfg.dl.btn') + '</button>';
              btns += '<button class="cfg-row-btn" onclick="cfgRename(\'' + sf + '\')">' + t('cfg.rename.btn') + '</button>';
              btns += '<button class="cfg-row-btn danger" onclick="cfgDelete(\'' + sf + '\')">✕ ' + t('cfg.destroy.btn') + '</button>';
            }
            btns += '</div>';

            return '<div class="' + cls + '">'
              + dot
              + '<span class="cfg-fname">' + f + '</span>'
              + btns
              + '</div>';
          }).join('');
        })
        .catch(function () {
          document.getElementById('cfg-filelist').textContent = '—';
        });
    }

    /* ── Per-row config actions ─────────────────────────────────────────── */

    function cfgChoose(name) {
      localStorage.setItem(CFG_PENDING_KEY, name);
      loadConfigs();
      markDirty();
    }

    function cfgSnapshot(name) {
      var base = name.replace(/\.json$/, '').substring(0, 13); // 13 + '_' + 13chars_ts + '.json' ≤ 32
      var toName = base + '_' + _cfgTs() + '.json';
      post('/api/config/copy', { from: name, to: toName })
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function () { cfgStatus('Snapshot → ' + toName, 'ok'); loadConfigs(); })
        .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
    }

    function cfgRename(name) {
      _cfgRenaming = name;
      loadConfigs();
      setTimeout(function () {
        var inp = document.getElementById('cfg-rename-inp');
        if (inp) { inp.focus(); inp.select(); }
      }, 0);
    }

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

    function cfgRenameCancel() {
      _cfgRenaming = null;
      loadConfigs();
    }

    function cfgDelete(name) {
      if (!confirm('Supprimer ' + name + ' ?')) return;
      deleteJson('/api/configs', { file: name })
        .then(function () { cfgStatus('Fichier supprimé.', 'ok'); loadConfigs(); })
        .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
    }

    /* ── Config management ──────────────────────────────────────────────── */

    function onCfgFileSelect(input) {
      var name = input.files.length ? input.files[0].name : t('cfg.ul.nofile');
      document.getElementById('cfg-filename').textContent = name;
      document.getElementById('cfg-upload-btn').disabled = !input.files.length;
      cfgStatus('', '');
    }

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

    function downloadConfig() { cfgDownload('config.json', _cfgActive); }

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

    function uploadConfig() {
      var file = document.getElementById('cfg-file').files[0];
      if (!file) return;

      var safeName = _sanitizeCfgName(file.name);

      var reader = new FileReader();
      reader.onload = function (e) {
        try { JSON.parse(e.target.result); }
        catch (err) { cfgStatus('JSON invalide : ' + err.message, 'err'); return; }

        document.getElementById('cfg-upload-btn').disabled = true;
        cfgStatus('Envoi en cours… → ' + safeName, 'ok');

        fetch('/api/configs?name=' + encodeURIComponent(safeName), {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', 'X-Config-Name': safeName },
          body: e.target.result
        })
          .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
          .then(function () { cfgStatus(t('cfg.saved'), 'ok'); document.getElementById('cfg-upload-btn').disabled = false; loadConfigs(); })
          .catch(function (err) { cfgStatus(t('de.err_prefix') + err.message, 'err'); });
      };
      reader.readAsText(file);
    }

    function applyEsp32() {
      cfgStatus(t('cfg.applying'), 'ok');
      var pending = localStorage.getItem(CFG_PENDING_KEY);
      var doRestart = function () {
        fetch('/api/restart', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' })
          .then(function () { startRebootCountdown(); })
          .catch(function () { startRebootCountdown(); });
      };
      if (pending) {
        post('/api/config/activate', { file: pending })
          .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
          .then(doRestart)
          .catch(function (e) { cfgStatus(t('de.err_prefix') + e.message, 'err'); });
      } else {
        doRestart();
      }
    }

    function startRebootCountdown() {
      var n = 8;
      function tick() {
        if (n > 0) {
          cfgStatus(t('cfg.rebooting', { n: n }), 'ok');
          n--;
          setTimeout(tick, 1000);
        } else {
          cfgStatus('Reconnexion…', 'ok');
          tryReconnect();
        }
      }
      tick();
    }

    function tryReconnect() {
      fetch('/api/status')
        .then(function (r) {
          if (r.ok) {
            clearDirty();
            history.replaceState(null, '', location.pathname + '#config');
            location.reload();
          } else {
            setTimeout(tryReconnect, 1000);
          }
        })
        .catch(function () { setTimeout(tryReconnect, 1000); });
    }

    function cfgStatus(msg, cls) {
      var el = document.getElementById('cfg-status');
      el.style.display = msg ? '' : 'none';
      el.className = 'cfg-status ' + cls;
      el.textContent = msg;
    }

    /* ── Dirty state ────────────────────────────────────────────────────── */

    var CFG_DIRTY_KEY = 'mrjfx_dirty';

    function markDirty() {
      localStorage.setItem(CFG_DIRTY_KEY, '1');
      renderDirtyBanner();
    }

    function clearDirty() {
      localStorage.removeItem(CFG_DIRTY_KEY);
      localStorage.removeItem(CFG_PENDING_KEY);
      _cfgSelected = null;
      renderDirtyBanner();
    }

    function renderDirtyBanner() {
      var dirty = !!localStorage.getItem(CFG_DIRTY_KEY);
      document.getElementById('dirty-banner').style.display = dirty ? '' : 'none';
    }

    /* ── Debug (mise au point) ──────────────────────────────────────────── */

    // Board type definitions — fetched from /api/board-types
    var _boardTypes = {};
    var _dbgBoards = [];  // from /api/boards
    var _dbgDevs = [];  // from /api/devices
    var _dbgTest = {};  // client GPIO test state: {'g17': 0|1}
    var _dbgTestSpi = {};  // client SPI test state: {'c1_p9': 0|1}
    var _dbgSysPins = {};  // gpio → sys label from config: {23:'MOSI', 18:'SCLK', ...}

    // Extract system-used GPIO labels from /api/config payload (buses format)
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
          if (bus.tx >= 0) _dbgSysPins[bus.tx] = shortName + '\u00b7TX';
          if (bus.rx >= 0) _dbgSysPins[bus.rx] = shortName + '\u00b7RX';
        }
      });
    }

    function loadDebug() {
      var pDevs = fetch('/api/devices')
        .then(function (r) { if (!r.ok) throw r; return r.json(); })
        .then(function (devs) { _dbgDevs = devs; })
        .catch(function () { });
      var pTypes = fetch('/api/board-types')
        .then(function (r) { if (!r.ok) throw r; return r.json(); })
        .then(function (bt) { _boardTypes = bt; })
        .catch(function () { });
      var pBoards = fetch('/api/boards')
        .then(function (r) { if (!r.ok) throw r; return r.json(); })
        .then(function (boards) { _dbgBoards = boards; })
        .catch(function () { });
      var pCfg = fetch('/api/config')
        .then(function (r) { if (!r.ok) throw r; return r.json(); })
        .then(function (cfg) { loadSystemPins(cfg); })
        .catch(function () { });
      Promise.all([pDevs, pTypes, pBoards, pCfg]).then(function () { renderDebugBoards(); });
    }

    function refreshDebug() { loadDebug(); }

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
      return null;
    }

    // ── Rendering ────────────────────────────────────────────────────────

    function renderDebugBoards() {
      var html = _dbgBoards.map(function (b, i) { return renderDbgBoard(b, i); }).join('');
      document.getElementById('dbg-boards').innerHTML =
        html || '<div class="prm-info">' + t('dbg.no_boards') + '</div>';
    }

    // board  — entry from /api/boards: { id, type, bus, pinCount, spiRank }
    // boardApiIdx — 0-based index in _dbgBoards (firmware uses boardApiIdx+1 for matching)
    function renderDbgBoard(board, boardApiIdx) {
      var def = _boardTypes[board.type];
      var badge = board.spiRank > 0
        ? ' <span class="dbg-idx-badge">board\u00a0' + board.spiRank + '</span>'
        : ' <span class="dbg-idx-badge">GPIO</span>';
      var name = tbt(board.type, 'label', (def && def.label) ? def.label : board.type);

      return '<div class="dbg-board">'
        + '<div class="dbg-board-hdr">'
        + '<span class="dbg-board-name">' + name + badge + '</span>'
        + '<div class="dbg-board-actions">'
        + '<button class="dbg-hbtn on"  onclick="dbgAll(' + boardApiIdx + ',1)">' + t('dbg.all_on') + '</button>'
        + '<button class="dbg-hbtn off" onclick="dbgAll(' + boardApiIdx + ',0)">' + t('dbg.all_off') + '</button>'
        + (def && def.rows > 0 ? '<button class="dbg-hbtn test" onclick="dbgAllTest(' + boardApiIdx + ')">' + t('dbg.all_test') + '</button>' : '')
        + '</div></div>'
        + (def ? renderDipPcb(board, boardApiIdx, def) : '')
        + '</div>';
    }

    // Render a board using its pins[] array from board_types.json.
    // Boards with rows=0 (UART chains, I²C modules…) show description only.
    function renderDipPcb(board, boardApiIdx, def) {
      if (!def.rows || def.rows === 0) {
        return '<div class="dbg-pcb"><div class="dbg-info">' + tbt(board.type, 'desc', def.description || '') + '</div></div>';
      }
      var leftPins = (def.pins || []).filter(function (p) { return p.side === 'left'; })
        .sort(function (a, b) { return a.row - b.row; });
      var rightPins = (def.pins || []).filter(function (p) { return p.side === 'right'; })
        .sort(function (a, b) { return a.row - b.row; });
      var leftCols = leftPins.map(function (p) { return renderPin(board, boardApiIdx, p); }).join('');
      var rightCols = rightPins.map(function (p) { return renderPin(board, boardApiIdx, p); }).join('');
      var shortLabel = tbt(board.type, 'label', def.label || board.type);
      return '<div class="dbg-pcb">'
        + '<div class="dbg-dip">'
        + '<div class="dbg-col">' + leftCols + '</div>'
        + '<div class="dbg-chip">' + shortLabel + '</div>'
        + '<div class="dbg-col">' + rightCols + '</div>'
        + '</div></div>';
    }

    // pin — entry from board_types.json pins[]: { wiring?, label, side, row, capabilities[] }
    // Pins without wiring are non-clickable (power, GND, EN, etc.)
    function renderPin(board, boardApiIdx, pin) {
      var caps = pin.capabilities || [];

      if (pin.wiring === undefined) {
        var sc = caps.indexOf('gnd') >= 0 ? 'gnd' : caps.indexOf('power') >= 0 ? 'pwr' : 'nc';
        return '<div class="dbg-pin ' + sc + '">' + pin.label + '</div>';
      }

      var num = pin.wiring;
      var isSpi = board.spiRank > 0;
      var dev = dbgFindDev(boardApiIdx, num);
      var cls = '', onclick = '', inner = '', ledBtn = '', editBtn = '';

      function mkLedBtn(gpio) {
        var ts = _dbgTest['g' + gpio] ? 1 : 0;
        return '<button class="dbg-led-btn ' + (ts ? 'on' : 'off') + '"'
          + ' onclick="event.stopPropagation();dbgTestGpio(' + gpio + ',' + (1 - ts) + ')"'
          + ' title="GPIO\u00a0' + gpio + ' direct">' + LED_ICO + '</button>';
      }

      function mkSpiLedBtn(card, ch) {
        var key = 'c' + card + '_p' + ch;
        var ts = _dbgTestSpi[key] ? 1 : 0;
        return '<button class="dbg-led-btn ' + (ts ? 'on' : 'off') + '"'
          + ' onclick="event.stopPropagation();dbgTestSpi(' + card + ',' + ch + ',' + (1 - ts) + ')"'
          + ' title="card\u00a0' + card + '\u00a0ch\u00a0' + ch + '">' + LED_ICO + '</button>';
      }

      if (dev) {
        cls = dev.desired > 0 ? 'on' : 'off';
        var ns = dev.desired > 0 ? 0 : 1;
        onclick = ' onclick="dbgToggleDev(\'' + dev.id + '\',' + ns + ')"';
        var ico = ICONS[dev.type] || ICONS['_'];
        var tip = tooltip(dev.type);
        inner = '<div class="dbg-pin-ico" title="' + tip + '">' + ico + '</div>'
          + '<span class="dbg-pin-num">' + num + '</span>';
        ledBtn = isSpi ? mkSpiLedBtn(board.spiRank, num) : mkLedBtn(num);
        editBtn = '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '" onclick="event.stopPropagation();openDevEditorById(\'' + dev.id + '\',' + boardApiIdx + ',' + num + ')">✎</button>';
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
        } else {
          var ts2 = _dbgTest['g' + num] ? 1 : 0;
          cls = ts2 ? 'test-on' : '';
          onclick = ' onclick="dbgTestGpio(' + num + ',' + (1 - ts2) + ')"';
          inner = '<span class="dbg-pin-num">' + num + '</span>';
          ledBtn = mkLedBtn(num);
          editBtn = '<button class="dbg-edit-btn" title="' + t('de.add_tip') + '" onclick="event.stopPropagation();openDevEditor(' + boardApiIdx + ',' + num + ',null)">+</button>';
        }
      } else {
        var spiKey = 'c' + board.spiRank + '_p' + num;
        var spiTs = _dbgTestSpi[spiKey] ? 1 : 0;
        cls = spiTs ? 'test-on' : '';
        onclick = ' onclick="dbgTestSpi(' + board.spiRank + ',' + num + ',' + (1 - spiTs) + ')"';
        inner = '<span class="dbg-pin-num">' + num + '</span>';
        ledBtn = mkSpiLedBtn(board.spiRank, num);
        editBtn = '<button class="dbg-edit-btn" title="' + t('de.add_tip') + '" onclick="event.stopPropagation();openDevEditor(' + boardApiIdx + ',' + num + ',null)">+</button>';
      }

      return '<div class="dbg-pin' + (cls ? ' ' + cls : '') + '"' + onclick + '>' + inner + ledBtn + editBtn + '</div>';
    }

    // ── Actions ──────────────────────────────────────────────────────────

    function dbgToggleDev(id, on) {
      post('/api/switch', { id: id, on: !!on })
        .then(function () { loadDebug(); })
        .catch(function (e) { console.error('dbgToggleDev', e); });
    }

    function setTheme(name) {
      document.body.classList.remove('th-amber', 'th-signal');
      if (name !== 'night') document.body.classList.add('th-' + name);
      localStorage.setItem('mrj-theme', name);
      document.querySelectorAll('.theme-dot').forEach(function (b) {
        var match = b.classList.contains('theme-dot-' + name);
        b.classList.toggle('active', match);
      });
    }

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

    // Sequential all-on/off — operates only on devices assigned to this board.
    // Also clears any active test state so the UI reflects real device state.
    function dbgAll(boardApiIdx, state) {
      var board = _dbgBoards[boardApiIdx];
      var clearCalls = [];
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
        post('/api/all', { state: state, board: boardApiIdx + 1 })
          .then(function () { loadDebug(); });
      });
    }

    // ALL test: turn off all devices on the card, then light every testable pin.
    function dbgAllTest(boardApiIdx) {
      var board = _dbgBoards[boardApiIdx];
      if (!board) return;
      post('/api/all', { state: 0, board: boardApiIdx + 1 })
        .then(function () {
          var calls = [];
          if (board.spiRank > 0) {
            // SPI board: test all channels 1..pinCount
            var card = board.spiRank;
            var pinCount = board.pinCount || 16;
            for (var p = 1; p <= pinCount; p++) {
              _dbgTestSpi['c' + card + '_p' + p] = 1;
              calls.push(fetch('/api/test/spi', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ card: card, channel: p, state: 1 })
              }));
            }
          } else {
            // GPIO board: test all output pins that are not system pins
            var def = _boardTypes[board.type];
            var pins = (def && def.pins) ? def.pins : [];
            pins.forEach(function (pin) {
              if (pin.wiring === undefined) return;
              if (_dbgSysPins[pin.wiring]) return;
              var caps = pin.capabilities || [];
              if (caps.indexOf('output') < 0) return;
              _dbgTest['g' + pin.wiring] = 1;
              calls.push(fetch('/api/test/gpio', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ pin: pin.wiring, state: 1 })
              }));
            });
          }
          return Promise.all(calls);
        })
        .then(function () { renderDebugBoards(); })
        .catch(function (e) { console.error('dbgAllTest', e); });
    }

    /* ── About ──────────────────────────────────────────────────────────── */

    function pad2(n) { return n < 10 ? '0' + n : String(n); }

    function fmtBytes(b) {
      if (b === undefined || b === null) return '\u2014';
      if (b >= 1048576) return (b / 1048576).toFixed(1) + '\u00a0MB';
      if (b >= 1024) return (b / 1024).toFixed(1) + '\u00a0KB';
      return b + '\u00a0B';
    }

    function fmtUptime(s) {
      var d = Math.floor(s / 86400);
      var h = Math.floor((s % 86400) / 3600);
      var m = Math.floor((s % 3600) / 60);
      var sec = s % 60;
      var str = pad2(h) + ':' + pad2(m) + ':' + pad2(sec);
      return d > 0 ? d + 'd\u00a0' + str : str;
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

    function renderAbout(s) {
      var html = '';

      // Firmware
      html += abtCard(t('abt.firmware'), [
        { label: t('abt.version'), value: s.version || '\u2014' },
        { label: t('abt.build'), value: s.build_date || '\u2014' },
        { label: t('abt.project'), value: t('abt.coming_soon') },
      ]);

      // System
      var chip = (s.chip || 'ESP32') + ' rev.\u00a0' + (s.chip_rev !== undefined ? s.chip_rev : '?');
      html += abtCard(t('abt.system'), [
        { label: t('abt.chip'), value: chip },
        { label: t('abt.cpu_freq'), value: (s.cpu_mhz || '\u2014') + '\u00a0MHz' },
        { label: t('abt.uptime'), value: fmtUptime(s.uptime_s || 0) },
      ]);

      // Memory
      var heapPct = s.heap_total ? Math.round((1 - s.heap_free / s.heap_total) * 100) : 0;
      html += abtCard(t('abt.memory'), [
        {
          label: t('abt.heap_free'),
          value: fmtBytes(s.heap_free) + '\u00a0/\u00a0' + fmtBytes(s.heap_total),
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
          value: fmtBytes(fwUsed) + '\u00a0/\u00a0' + fmtBytes(fwTotal),
          bar: fwPct
        },
      ]);

      // Filesystem
      if (s.fs_total !== undefined) {
        var fsPct = s.fs_total ? Math.round(s.fs_used / s.fs_total * 100) : 0;
        html += abtCard(t('abt.fs'), [
          {
            label: 'LittleFS',
            value: fmtBytes(s.fs_used) + '\u00a0/\u00a0' + fmtBytes(s.fs_total),
            bar: fsPct
          },
        ]);
      }

      // Temperature
      if (s.temp_c !== undefined) {
        var tempPct = Math.min(100, Math.max(0, Math.round((s.temp_c - 20) * 100 / 80)));
        html += abtCard(t('abt.temp'), [
          {
            label: 'CPU',
            value: s.temp_c.toFixed(1) + '\u00a0\u00b0C',
            bar: tempPct
          },
        ]);
      }

      document.getElementById('abt-grid').innerHTML = html;
    }

    /* ── Params ─────────────────────────────────────────────────────────── */
    var _pollTimer = null;

    function savePollInterval() {
      var v = parseInt(document.getElementById('prm-poll').value, 10);
      if (v >= 500) {
        POLL = v;
        localStorage.setItem('poll', String(v));
        if (_pollTimer) clearInterval(_pollTimer);
        _pollTimer = setInterval(poll, POLL);
      }
    }

    function loadParams() {
      // Nothing to load from server anymore — only local UI settings
    }


    /* ── Device editor ──────────────────────────────────────────────────── */

    var DEV_TYPES = [
      'Beacon','CampFire','DefectLamp','DfAudio','DoubleBeacon','ElectricLamp',
      'GasLamp','MrJDBBlocSignal','MrJDBEntrySignal','MrJDBExitSignal','NeonSign','OilLamp',
      'RailwayCrossingLights','SerialServo','SignalFlare','SolderLamp','StaticLow','Storm',
      'Torch','TrafficLight3ph','TrafficLight4ph','TrainHeadLamp','TurnSignal'
    ];

    var DE_WIRING = {
      'DoubleBeacon':2, 'RailwayCrossingLights':2, 'MrJDBBlocSignal':2,
      'MrJDBEntrySignal':3, 'TrafficLight3ph':3, 'TrafficLight4ph':3,
      'MrJDBExitSignal':4, 'DfAudio':0
    };

    var _deEditId = null;

    function openDevEditorById(id, boardApiIdx, pin) {
      for (var i = 0; i < _dbgDevs.length; i++) {
        if (_dbgDevs[i].id === id) { openDevEditor(boardApiIdx, pin, _dbgDevs[i]); return; }
      }
      openDevEditor(boardApiIdx, pin, null);
    }

    function openDevEditor(boardApiIdx, prefillPin, dev) {
      _deEditId = dev ? dev.id : null;

      // Type select
      var typeEl = document.getElementById('de-type');
      typeEl.innerHTML = DEV_TYPES.map(function (tp) {
        return '<option value="' + tp + '">' + tp + ' — ' + tooltip(tp) + '</option>';
      }).join('');

      // Board select
      var boardEl = document.getElementById('de-board');
      boardEl.innerHTML = _dbgBoards.map(function (b, i) {
        var badge = b.spiRank > 0 ? 'SPI board\u00a0' + b.spiRank : 'GPIO';
        return '<option value="' + i + '">' + b.id + ' (' + badge + ')</option>';
      }).join('');

      if (dev) {
        document.getElementById('de-title').textContent = t('de.edit_prefix') + dev.id;
        document.getElementById('de-id').value = dev.id;
        typeEl.value = dev.type;
        boardEl.value = dev.board - 1;
        document.getElementById('de-addr').value = dev.addr > 0 ? dev.addr : '';
        document.getElementById('de-defstate').value = dev.desired > 0 ? 'on' : '';
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
      deStatus('', '');
      document.getElementById('de-save-btn').disabled = false;

      document.getElementById('de-overlay').style.display = 'block';
      document.getElementById('de-modal').style.display = 'flex';
      applyLang();
    }

    function closeDevEditor() {
      document.getElementById('de-overlay').style.display = 'none';
      document.getElementById('de-modal').style.display = 'none';
    }

    function deUpdateWiring(prefillPin, dev) {
      var type = document.getElementById('de-type').value;
      var count = DE_WIRING[type] !== undefined ? DE_WIRING[type] : 1;
      var grp = document.getElementById('de-wiring-grp');
      if (count === 0) { grp.innerHTML = ''; return; }

      // Preserve currently displayed values when called from onchange (no args)
      var existingInputs = grp.querySelectorAll('.de-w');
      var existingVals = [];
      existingInputs.forEach(function(inp) {
        var v = parseInt(inp.value, 10);
        if (!isNaN(v)) existingVals.push(v);
      });

      var pins;
      if (dev) {
        pins = dev.pins || [];
      } else if (prefillPin !== undefined) {
        pins = [prefillPin];
      } else {
        pins = existingVals; // preserve on type change
      }

      // Build wiring datalist from available wirings on selected board
      var boardIdx = parseInt(document.getElementById('de-board').value, 10);
      var board = _dbgBoards[boardIdx];
      var dlId = 'de-wiring-list';
      var dlHtml = '<datalist id="' + dlId + '">';
      if (board) {
        var bt = _boardTypes[board.type];
        var usedPins = {};
        _dbgDevs.forEach(function(d) {
          if (d.board === boardIdx + 1 && d.id !== _deEditId)
            (d.pins || []).forEach(function(p) { usedPins[p] = true; });
        });
        if (bt && bt.pins) {
          bt.pins.forEach(function(p) {
            if (p.wiring !== undefined && !usedPins[p.wiring]) {
              dlHtml += '<option value="' + p.wiring + '">';
            }
          });
        }
      }
      dlHtml += '</datalist>';

      var html = dlHtml + '<div class="de-field"><label>' + t('de.lbl_wiring') + '</label><div class="de-wiring-row">';
      for (var i = 0; i < count; i++) {
        html += '<input type="number" class="de-w" list="' + dlId + '" min="0" max="253"'
          + ' placeholder="pin' + (count > 1 ? '\u00a0' + (i + 1) : '') + '"'
          + ' value="' + (pins[i] !== undefined ? pins[i] : '') + '">';
      }
      html += '</div></div>';
      grp.innerHTML = html;
    }

    function deStatus(msg, cls) {
      var el = document.getElementById('de-status');
      el.style.display = msg ? '' : 'none';
      el.className = 'de-status ' + cls;
      el.textContent = msg;
    }

    function saveDevEditor() {
      var id = (document.getElementById('de-id').value || '').trim();
      var type = document.getElementById('de-type').value;
      var boardIdx = parseInt(document.getElementById('de-board').value, 10);
      var addrStr = (document.getElementById('de-addr').value || '').trim();
      var defState = document.getElementById('de-defstate').value;
      var count = DE_WIRING[type] !== undefined ? DE_WIRING[type] : 1;

      if (!id) { deStatus(t('de.err_id'), 'err'); return; }

      var wiring = [];
      if (count > 0) {
        var wInputs = document.querySelectorAll('.de-w');
        for (var i = 0; i < wInputs.length; i++) {
          var v = parseInt(wInputs[i].value, 10);
          if (isNaN(v) || v < 0 || v > 253) { deStatus(t('de.err_wiring'), 'err'); return; }
          wiring.push(v);
        }
      }

      var board = _dbgBoards[boardIdx];
      if (!board) { deStatus(t('de.err_board'), 'err'); return; }

      var dev = { id: id, type: type, board: board.id };
      if (count === 1) dev.wiring = wiring[0];
      else if (count > 1) dev.wiring = wiring;
      if (addrStr) { var addr = parseInt(addrStr, 10); if (addr >= 1 && addr <= 10239) dev.address = addr; }
      if (defState) dev.default_state = defState;

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
          deStatus(t('de.saved'), 'ok');
          document.getElementById('de-save-btn').disabled = false;
          markDirty();
        })
        .catch(function (e) {
          if (e) { deStatus(t('de.err_prefix') + e.message, 'err'); document.getElementById('de-save-btn').disabled = false; }
        });
    }

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
          deStatus(t('de.deleted'), 'ok');
          markDirty();
        })
        .catch(function (e) { deStatus(t('de.err_prefix') + e.message, 'err'); });
    }

    /* ── Boot ───────────────────────────────────────────────────────────── */
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
    applyLang();
    (function () {
      var h = location.hash.slice(1);
      var valid = ['cockpit', 'canvas', 'params', 'config', 'about'];
      if (h && valid.indexOf(h) >= 0) switchView(h);
    })();
    window.addEventListener('hashchange', function () {
      var h = location.hash.slice(1);
      if (h && h !== _currentView && document.getElementById('view-' + h)) switchView(h);
    });
    poll();
    _pollTimer = setInterval(poll, POLL);

