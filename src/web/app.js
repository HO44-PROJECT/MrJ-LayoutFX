
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
      if (name === 'config') { renderDirtyBanner(); switchCfgTab(_currentCfgTab); }
    }

    var _currentCfgTab = 'boards';

    function switchCfgTab(name) {
      _currentCfgTab = name;
      document.querySelectorAll('.cfg-tab').forEach(function (t) {
        t.classList.toggle('active', t.dataset.tab === name);
      });
      document.querySelectorAll('.cfg-tabpanel').forEach(function (p) {
        p.classList.toggle('active', p.id === 'cfg-tab-' + name);
      });
      if (name === 'boards') { loadDebug(); }
      if (name === 'buses')  { loadDebug(); }
      if (name === 'files')  { loadConfigs(); cfgStatus('', ''); }
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
      var h = '<div class="meta">';
      if (d.addr > 0) h += '<span class="mtag">DCC ' + d.addr + '</span>';
      if (d.servoId !== undefined) {
        // UART servo: no GPIO pins, just a bus ID
        var boardLabel = d.board > 0 ? 'Carte ' + d.board + ' \u00b7 ' : '';
        h += '<span class="mtag">' + boardLabel + t('de.lbl_wiring_servo') + '\u00a0' + d.servoId + '</span>';
      } else {
        var pins = d.pins ? d.pins.filter(function (p) { return p !== 255; }) : [];
        var pinStr = pins.length > 0 ? pins.join(',\u202f') : null;
        if (d.board > 0) h += '<span class="mtag">Carte ' + d.board + (pinStr ? ' \u00b7 pin\u00a0' + pinStr : '') + '</span>';
        else if (pinStr) h += '<span class="mtag">GPIO\u00a0' + pinStr + '</span>';
      }
      return h + '</div>';
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

    var _ckAllDevs    = [];
    var _ckTypeFilter = null;

    function ckBuildTypeFilters(devs) {
      var seen = [];
      devs.forEach(function(d) { if (seen.indexOf(d.type) < 0) seen.push(d.type); });
      var html = seen.map(function(type) {
        var ico = ICONS[type] || ICONS['_'];
        var tip = tooltip(type);
        var active = _ckTypeFilter === type ? ' ck-tf-active' : '';
        var label = type.replace(/^MrJDB/, '').replace(/([A-Z])/g, ' $1').trim();
        return '<button class="ck-tf' + active + '" title="' + tip + '" onclick="ckToggleType(\'' + type + '\')">'
          + ico + '<span class="ck-tf-lbl">' + label + '</span></button>';
      }).join('');
      document.getElementById('ck-type-filters').innerHTML = html;
    }

    function ckToggleType(type) {
      _ckTypeFilter = (_ckTypeFilter === type) ? null : type;
      ckApplyFilters();
    }

    function ckApplyFilters() {
      var search = (document.getElementById('ck-search').value || '').trim().toLowerCase();
      var devs = _ckAllDevs.filter(function(d) {
        if (_ckTypeFilter && d.type !== _ckTypeFilter) return false;
        if (search && d.id.toLowerCase().indexOf(search) < 0) return false;
        return true;
      });
      ckBuildTypeFilters(_ckAllDevs);
      renderGrid(devs);
    }

    function renderGrid(devs) {
      var order = [];
      var groups = {};
      devs.forEach(function (d) {
        if (!groups[d.type]) { groups[d.type] = []; order.push(d.type); }
        groups[d.type].push(d);
      });
      var html = '';
      if (devs.length === 0 && _ckAllDevs.length > 0) {
        html = '<div class="prm-info">' + t('ck.no_match') + '</div>';
      }
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
    }

    function render(devs) {
      _ckAllDevs = devs;
      var toolbar = document.getElementById('ck-toolbar');
      var grid    = document.getElementById('grid');
      if (devs.length === 0) {
        toolbar.style.display = 'none';
        grid.innerHTML = '<div class="ck-empty">'
          + '<div class="ck-empty-ico">🎛</div>'
          + '<div class="ck-empty-title">' + t('ck.empty_title') + '</div>'
          + '<div class="ck-empty-body">' + t('ck.empty_body') + '</div>'
          + '<button class="ck-empty-btn" onclick="switchView(\'config\')">' + t('ck.empty_btn') + '</button>'
          + '</div>';
      } else {
        toolbar.style.display = '';
        ckBuildTypeFilters(devs);
        ckApplyFilters();
      }
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

    var _firstPollDone = false;

    function poll() {
      fetch('/api/devices')
        .then(function (r) { if (!r.ok) throw r; return r.json(); })
        .then(function (d) {
          document.getElementById('err').style.display = 'none';
          render(d);
          if (_currentView === 'config') { _dbgDevs = d; renderDebugBoards(); }
          if (!_firstPollDone) {
            _firstPollDone = true;
            if (d.length === 0) showWelcome();
          }
        })
        .catch(showErr);
    }

    function showWelcome() {
      // Don't show if user already navigated to a specific view via URL hash
      if (location.hash && location.hash !== '#cockpit') return;

      // Fetch config (required) and status (for env detection, best-effort)
      var pStatus = _dbgStatus
        ? Promise.resolve(_dbgStatus)
        : fetch('/api/status').then(function (r) { return r.json(); }).catch(function () { return {}; });

      Promise.all([
        fetch('/api/config').then(function (r) {
          if (r.status === 404) return { buses: {}, boards: [], devices: [] };
          if (!r.ok) throw new Error('cfg ' + r.status);
          return r.json();
        }),
        pStatus
      ])
        .then(function (results) {
          var cfg    = results[0];
          var status = results[1];
          _dbgCfg = cfg;
          if (status && status.env) _dbgStatus = status;
          loadSystemPins(cfg);

          if (cfg.boards && cfg.boards.length > 0) return; // already set up — nothing to do

          // Determine default MCU board type from env, fall back to ESP32DevkitC
          var env = (_dbgStatus && _dbgStatus.env || '').toLowerCase().replace(/[_\s-]/g, '');
          var defaultType = _ENV_TO_BOARD[env] || 'ESP32DevkitC';
          // If board_types is already loaded, prefer an MCU type (no busType) over the fallback
          var btKeys = Object.keys(_boardTypes).filter(function (k) { return !(_boardTypes[k].busType); });
          if (btKeys.length > 0 && !_boardTypes[defaultType]) defaultType = btKeys[0];

          var boardId = defaultType.toLowerCase().replace(/[^a-z0-9]/g, '');
          cfg.boards = [{ id: boardId, type: defaultType }];

          return fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg)
          })
            .then(function (r) { if (!r.ok) throw new Error('save ' + r.status); return r.json(); })
            .then(function () {
              _dbgCfg = cfg;
              document.getElementById('welcome-overlay').style.display = 'block';
              document.getElementById('welcome-modal').style.display = 'flex';
              applyLang();
            });
        })
        .catch(function (e) { console.error('[welcome]', e); });
    }

    function closeWelcome() {
      document.getElementById('welcome-overlay').style.display = 'none';
      document.getElementById('welcome-modal').style.display = 'none';
      loadDebug();
      switchView('config');
    }

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

    function exportCode() {
      fetch('/api/export/code')
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
        .then(function (code) {
          var blob = new Blob([code], { type: 'text/plain' });
          var url  = URL.createObjectURL(blob);
          var a    = document.createElement('a');
          a.href     = url;
          a.download = 'main.cpp';
          document.body.appendChild(a);
          a.click();
          document.body.removeChild(a);
          URL.revokeObjectURL(url);
        })
        .catch(function (e) { alert(t('cfg.export_err') + ': ' + e.message); });
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

    function showRebootModal() {
      document.getElementById('reboot-overlay').classList.add('active');
      document.getElementById('reboot-modal').style.display = 'block';
    }

    function hideRebootModal() {
      document.getElementById('reboot-overlay').classList.remove('active');
      document.getElementById('reboot-modal').style.display = 'none';
    }

    function startRebootCountdown() {
      showRebootModal();
      var n = 8;
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
    var _dbgDevs = [];   // from /api/devices (runtime)
    var _dbgCfg = null;  // from /api/config (persisted config)
    var _dbgStatus = null; // from /api/status
    var _busDev = {};    // merged bus devices cache: {id → mergedDev}
    var _dbgTest = {};  // client GPIO test state: {'g17': 0|1}
    var _dbgTestSpi = {};  // client SPI test state: {'c1_p9': 0|1}
    var _dbgSysPins = {};  // gpio → sys label from config: {23:'MOSI', 18:'SCLK', ...}
    var _dbgFirmwarePins = {};  // gpio → label from compile-time features (api/status sys_pins)

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
      Promise.all([pDevs, pTypes, pBoards, pCfg, pStatus]).then(function () {
        // Merge firmware-reserved pins — config-declared buses take precedence
        Object.keys(_dbgFirmwarePins).forEach(function (gpio) {
          if (!_dbgSysPins[gpio]) _dbgSysPins[gpio] = _dbgFirmwarePins[gpio];
        });
        applyLayoutName((_dbgCfg && _dbgCfg.name) || '');
        renderDebugBoards();
        if (_currentCfgTab === 'buses') renderBusesTab();
        var exportBtn = document.getElementById('cfg-export-btn');
        if (exportBtn) exportBtn.style.display = (_dbgStatus && _dbgStatus.ip === 'localhost') ? '' : 'none';
      });
    }

    function refreshDebug() { loadDebug(); }

    // ── I2C scanner ──────────────────────────────────────────────────────────

    var I2C_KNOWN = {
      0x20: 'PCF8574', 0x21: 'PCF8574', 0x22: 'PCF8574', 0x23: 'PCF8574',
      0x24: 'PCF8574', 0x25: 'PCF8574', 0x26: 'PCF8574', 0x27: 'PCF8574',
      0x38: 'PCF8574A', 0x39: 'PCF8574A', 0x3A: 'PCF8574A', 0x3B: 'PCF8574A',
      0x3C: 'SSD1306/SH1106', 0x3D: 'SSD1306',
      0x40: 'INA219/PCA9685', 0x48: 'ADS1115/PCF8591',
      0x57: 'EEPROM/DS3231', 0x68: 'DS3231/MPU6050', 0x69: 'MPU6050',
      0x76: 'BME280/BMP280', 0x77: 'BME280/BMP280'
    };

    function scanI2c() {
      var btn = document.getElementById('i2c-scan-btn');
      var res = document.getElementById('i2c-result');
      btn.disabled = true;
      btn.textContent = t('dbg.scan_i2c_scanning');
      res.style.display = 'none';
      fetch('/api/scan/i2c')
        .then(function(r) { return r.json(); })
        .then(function(d) {
          var sda = d.sda !== undefined ? d.sda : '?';
          var scl = d.scl !== undefined ? d.scl : '?';
          var html = '<span class="i2c-pins">SDA\u00a0GPIO' + sda + ' / SCL\u00a0GPIO' + scl + '</span> ';
          if (!d.found || d.found.length === 0) {
            html += '<span class="i2c-none">' + t('dbg.scan_i2c_none') + '</span>';
          } else {
            html += '<span class="i2c-label">' + t('dbg.scan_i2c_found') + ':</span> ';
            html += d.found.map(function(a) {
              var hex = '0x' + ('0' + a.toString(16).toUpperCase()).slice(-2);
              var name = I2C_KNOWN[a] ? ' <span class="i2c-name">' + I2C_KNOWN[a] + '</span>' : '';
              return '<span class="i2c-addr">' + hex + name + '</span>';
            }).join(' ');
          }
          res.innerHTML = html;
          res.style.display = 'flex';
        })
        .catch(function(e) {
          res.innerHTML = '<span class="i2c-none">Erreur: ' + e.message + '</span>';
          res.style.display = 'flex';
        })
        .finally(function() {
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
        if (match) return { id: cd.id, type: cd.type, desired: -1, pins: [wiring], _cfgOnly: true };
      }
      return null;
    }

    // ── Rendering ────────────────────────────────────────────────────────

    function applyLayoutName(name) {
      var span = document.getElementById('hdr-layout-name');
      if (span) span.textContent = name ? '\u00a0\u2014\u00a0' + name : '';
      var inp = document.getElementById('cfg-layout-name');
      if (inp && inp !== document.activeElement) inp.value = name || '';
    }

    function onLayoutNameInput() {
      var name = (document.getElementById('cfg-layout-name').value || '').trim();
      applyLayoutName(name);
      if (!_dbgCfg) return;
      _dbgCfg.name = name || undefined;
      markDirty();
      saveCfg(_dbgCfg);
    }

    function saveCfg(cfg) {
      fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(cfg) })
        .then(function(r) { if (r.ok) clearDirty(); })
        .catch(function() {});
    }

    function renderDebugBoards() {
      // Use config boards as source of truth (reflects saves immediately).
      // Overlay runtime data (spiRank, pinCount) from _dbgBoards when available.
      var cfgBoards = (_dbgCfg && _dbgCfg.boards) || [];
      var boards = cfgBoards.map(function (cb, cfgIdx) {
        var rt = null;
        for (var k = 0; k < _dbgBoards.length; k++) {
          if (_dbgBoards[k].id === cb.id) { rt = _dbgBoards[k]; break; }
        }
        return {
          id:       cb.id,
          type:     cb.type || '',
          bus:      cb.bus  || '',
          pinCount: rt ? rt.pinCount : (cb.pin_count || 0),
          spiRank:  rt ? rt.spiRank  : 0,
          _cfgIdx:  cfgIdx
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
        ? ' <span class="dbg-idx-badge">board\u00a0' + board.spiRank + '</span>'
        : ' <span class="dbg-idx-badge">GPIO</span>';
      var name = tbt(board.type, 'label', (def && def.label) ? def.label : board.type);

      var cfgIdx = board._cfgIdx !== undefined ? board._cfgIdx : -1;
      return '<div class="dbg-board">'
        + '<div class="dbg-board-hdr">'
        + '<span class="dbg-board-name">' + name + badge + '</span>'
        + '<div class="dbg-board-actions">'
        + '<button class="dbg-hbtn on"  onclick="dbgAll(' + boardApiIdx + ',1)">' + t('dbg.all_on') + '</button>'
        + '<button class="dbg-hbtn off" onclick="dbgAll(' + boardApiIdx + ',0)">' + t('dbg.all_off') + '</button>'
        + (def && def.rows > 0 ? '<button class="dbg-hbtn test" onclick="dbgAllTest(' + boardApiIdx + ')">' + t('dbg.all_test') + '</button>' : '')
        + (cfgIdx >= 0 ? '<button class="dbg-hbtn" onclick="openBoardEditor(' + cfgIdx + ')">' + t('be.edit') + '</button>' : '')
        + (cfgIdx >= 0 ? '<button class="dbg-hbtn off" onclick="deleteBoard(\'' + board.id.replace(/'/g, "\\'") + '\')">' + t('de.del') + '</button>' : '')
        + '</div></div>'
        + (def ? renderDipPcb(board, boardApiIdx, def) : '')
        + '</div>';
    }

    // Render a board using its pins[] array from board_types.json.
    // Boards with rows=0 (UART chains, I²C modules…) show device list instead of DIP pins.
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
            state:   rt ? rt.state   : 0,
            addr:    cfgDev.address || 0,
            pins:    rt ? (rt.pins || []) : []
          };
          _busDev[cfgDev.id] = merged; // cache for openDevEditorById
          return merged;
        });
        return '<div class="dbg-pcb">'
          + '<div class="dbg-info">' + tbt(board.type, 'desc', def.description || '') + '</div>'
          + devs.map(function (d) { return renderBusDevice(boardApiIdx, d); }).join('')
          + '<div class="dbg-bus-add"><button class="dbg-hbtn" onclick="openDevEditor('
          + boardApiIdx + ',1,null,SERVO_TYPES)">' + t('de.add_btn') + '</button></div>'
          + '</div>';
      }
      var allPins = def.pins || [];
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
      var usbElRight  = '<div class="dbg-usb-right">'  + usbSvg + '<span>USB</span></div>';
      var usbElBottom = '<div class="dbg-usb-bottom">' + usbSvg + '<span>USB</span></div>';
      var dip = '<div class="dbg-dip">';
      function wrapChip(chipHtml) {
        if (usbSide === 'right') {
          return '<div class="dbg-chip-row"><div class="dbg-chip">' + chipHtml + '</div>' + usbElRight + '</div>';
        }
        return '<div class="dbg-chip">' + chipHtml + '</div>';
      }
      if (hasMultiCol) {
        // outer row first (col:2), then inner row (col:1), then chip, then inner, then outer
        dip += '<div class="dbg-col">' + colRow('left', 2)  + '</div>';
        dip += '<div class="dbg-col">' + colRow('left', 1)  + '</div>';
        dip += wrapChip(shortLabel);
        dip += '<div class="dbg-col">' + colRow('right', 1) + '</div>';
        dip += '<div class="dbg-col">' + colRow('right', 2) + '</div>';
      } else {
        dip += '<div class="dbg-col">' + colRow('left', 1)  + '</div>';
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
      html += '<span class="dbg-bus-id">' + dev.id + '</span>';
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
          html += '<button class="dbg-hbtn ' + s.c + '" onclick="' + action + '">' + s.l + '</button>';
        });
      }
      html += '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '"'
        + ' onclick="openDevEditorById(\'' + sf + '\',' + boardApiIdx + ',1,SERVO_TYPES)">&#9998;</button>';
      html += '<button class="dbg-del-btn" title="' + t('de.del') + '"'
        + ' onclick="deleteBusDev(\'' + sf + '\')">&#10005;</button>';
      html += '</div>';
      return html;
    }

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
        .then(function () { markDirty(); loadDebug(); })
        .catch(function (e) { alert(t('de.err_prefix') + e.message); });
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

      if (dev && dev._cfgOnly) {
        cls = 'cfg';
        var ico = ICONS[dev.type] || ICONS['_'];
        var tip = tooltip(dev.type);
        inner = '<div class="dbg-pin-ico" title="' + tip + '">' + ico + '</div>'
          + '<span class="dbg-pin-num">' + num + '</span>';
        editBtn = '<button class="dbg-edit-btn" title="' + t('de.edit_tip') + '" onclick="event.stopPropagation();openDevEditorById(\'' + dev.id + '\',' + boardApiIdx + ',' + num + ')">✎</button>';
      } else if (dev) {
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
        } else if (caps.indexOf('output') < 0) {
          cls = 'nc';
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

    var _featTipEl = null;
    function showFeatTip(e, text) {
      e.stopPropagation();
      if (!_featTipEl) _featTipEl = document.getElementById('abt-tooltip');
      _featTipEl.textContent = text;
      _featTipEl.style.display = 'block';
      var r = e.target.getBoundingClientRect();
      var tw = _featTipEl.offsetWidth;
      var left = Math.min(r.left, window.innerWidth - tw - 8);
      var top  = r.bottom + 6;
      if (top + _featTipEl.offsetHeight > window.innerHeight - 8)
        top = r.top - _featTipEl.offsetHeight - 6;
      _featTipEl.style.left = Math.max(8, left) + 'px';
      _featTipEl.style.top  = top + 'px';
    }
    function hideFeatTip() {
      if (_featTipEl) _featTipEl.style.display = 'none';
    }
    document.addEventListener('click', hideFeatTip);

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
        { label: t('abt.env'), value: s.env || '\u2014' },
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

      // Configuration
      if (s.devices !== undefined) {
        var devMax = s.devices_max || 0;
        var devPct = devMax ? Math.round(s.devices / devMax * 100) : 0;
        html += abtCard(t('abt.config'), [
          {
            label: t('abt.devices'),
            value: s.devices + '\u00a0/\u00a0' + devMax,
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
            value: s.temp_c.toFixed(1) + '\u00a0\u00b0C',
            bar: tempPct
          },
        ]);
      }

      // Features
      if (s.features) {
        var FEAT_LABELS = {
          api: 'API', audio: 'Audio', config: 'Config', dcc: 'DCC',
          lobot_servo: 'Lobot Servo', lx16a_servo: 'LX-16A Servo',
          oled: 'OLED', spi: 'SPI', webui: 'WebUI', wifi: 'WiFi'
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
          id:      bd.id,
          type:    bd.type,
          board:   bd.board,
          servoId: bd.servoId,
          desired: rt ? rt.desired : bd.desired,
          state:   rt ? rt.state   : bd.state,
          addr:    bd.addr,
          pins:    bd.pins
        };
        openDevEditor(boardApiIdx, pin, merged, typeFilter);
        return;
      }
      // Non-bus devices: use runtime dev directly
      for (var j = 0; j < _dbgDevs.length; j++) {
        if (_dbgDevs[j].id === id) { openDevEditor(boardApiIdx, pin, _dbgDevs[j], typeFilter); return; }
      }
      openDevEditor(boardApiIdx, pin, null, typeFilter);
    }

    function openDevEditor(boardApiIdx, prefillPin, dev, typeFilter) {
      _deEditId = dev ? dev.id : null;

      // Type select — filtered if typeFilter provided (e.g. SERVO_TYPES for bus boards)
      var typeEl = document.getElementById('de-type');
      var typeList = typeFilter || DEV_TYPES;
      typeEl.innerHTML = typeList.map(function (tp) {
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
      deUpdateAddrLabel();
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

    function deUpdateAddrLabel() {
      var isServo = SERVO_TYPES.indexOf(document.getElementById('de-type').value) >= 0;
      // For servo: bus ID is in the wiring field — hide the DCC addr field entirely
      var addrField = document.querySelector('#de-modal #de-addr');
      var addrFieldRow = addrField ? addrField.closest('.de-field') : null;
      if (addrFieldRow) addrFieldRow.style.display = isServo ? 'none' : '';
      if (!isServo) {
        var lbl  = document.querySelector('#de-modal .de-addr-lbl');
        var hint = document.querySelector('#de-modal .de-addr-hint');
        if (lbl)  lbl.textContent  = t('de.lbl_addr');
        if (hint) hint.textContent = t('de.lbl_addr_hint');
        if (addrField) { addrField.min = 1; addrField.max = 10239; }
      }
    }

    function deUpdateWiring(prefillPin, dev) {
      var type = document.getElementById('de-type').value;
      var isServo = SERVO_TYPES.indexOf(type) >= 0;
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
            + ' placeholder="ID' + (count > 1 ? '\u00a0' + (i + 1) : '') + '"'
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
          Object.keys(_dbgSysPins || {}).forEach(function(g) { usedPins[parseInt(g)] = true; });
          var cfgBoard2Id = _dbgCfg && _dbgCfg.boards && _dbgCfg.boards[boardIdx2]
            ? _dbgCfg.boards[boardIdx2].id : (board2.id || '');
          ((_dbgCfg && _dbgCfg.devices) || []).forEach(function(d) {
            if (d.board !== cfgBoard2Id || d.id === _deEditId) return;
            var w = d.wiring;
            (Array.isArray(w) ? w : [w]).forEach(function(p) { if (p !== undefined) usedPins[parseInt(p)] = true; });
          });
          if (bt2 && bt2.pins) {
            bt2.pins.forEach(function(p) {
              if (p.wiring === undefined) return;
              var caps = p.capabilities || [];
              if (caps.indexOf('output') < 0) return; // must be driveable
              availOpts.push({ val: p.wiring, label: p.label, used: !!usedPins[p.wiring] });
            });
            availOpts.sort(function(a, b) { return a.val - b.val; });
          }
        }
        for (var i = 0; i < count; i++) {
          var curVal = pins[i] !== undefined ? pins[i] : '';
          var selLabel = count > 1 ? ' (' + (i + 1) + ')' : '';
          html += '<select class="de-w" onchange="deUpdateIdPlaceholder()">';
          html += '<option value="">— pin' + selLabel + ' —</option>';
          availOpts.forEach(function(o) {
            if (o.used && o.val !== curVal) return;
            var sel = (o.val === curVal) ? ' selected' : '';
            html += '<option value="' + o.val + '"' + sel + '>' + o.val + '</option>';
          });
          html += '</select>';
        }
      }

      html += '</div></div>';
      grp.innerHTML = html;
      deUpdateIdPlaceholder();
    }

    function deUpdateIdPlaceholder() {
      var idEl = document.getElementById('de-id');
      if (!idEl || idEl.value.trim()) return;
      var type = (document.getElementById('de-type') || {}).value || '';
      var count = DE_WIRING[type] !== undefined ? DE_WIRING[type] : 1;
      var firstPinEl = document.querySelector('.de-w');
      var firstPin = (count > 0 && firstPinEl) ? (parseInt(firstPinEl.value, 10) || '') : '';
      var shortType = type.replace(/^MrJDB/, '').toLowerCase().replace(/[^a-z0-9]/g, '');
      idEl.placeholder = shortType ? shortType + (firstPin !== '' ? firstPin : '') : 'auto';
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
          markDirty();
          closeDevEditor();
          loadDebug();
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
          markDirty();
          closeDevEditor();
          loadDebug();
        })
        .catch(function (e) { deStatus(t('de.err_prefix') + e.message, 'err'); });
    }

    /* ── Board editor ───────────────────────────────────────────────────── */

    var _beEditIdx = -1; // index in _dbgCfg.boards[], -1 = new

    // Map PlatformIO env names to board type keys in board_types.json
    var _ENV_TO_BOARD = {
      'esp32devkitc':          'ESP32DevkitC',
      'esp32devkitcbreadboard': 'ESP32DevkitC',
      'esp32minibreadboard':   'ESP32Mini',
      'esp32mini':             'ESP32Mini'
    };

    function guessDefaultBoardType() {
      var env = (_dbgStatus && _dbgStatus.env || '').toLowerCase().replace(/[_\s-]/g, '');
      var matched = _ENV_TO_BOARD[env];
      if (matched && _boardTypes[matched]) return matched;
      return Object.keys(_boardTypes)[0] || 'ESP32DevkitC';
    }

    function openBoardEditor(cfgIdx) {
      _beEditIdx = cfgIdx !== null ? cfgIdx : -1;
      var board = (cfgIdx !== null && cfgIdx >= 0 && _dbgCfg) ? _dbgCfg.boards[cfgIdx] : null;

      // Populate type select — disable types whose required bus has no instance in config
      var typeEl = document.getElementById('be-type');
      var buses = (_dbgCfg && _dbgCfg.buses) || {};
      typeEl.innerHTML = Object.keys(_boardTypes).map(function (k) {
        var def = _boardTypes[k];
        var required = def.busType || null;
        var hasCompatBus = !required || Object.keys(buses).some(function (bk) {
          return buses[bk].type === required;
        });
        var dis = hasCompatBus ? '' : ' disabled title="' + t('be.no_bus_warn').replace(/"/g, '&quot;') + '"';
        var label = (def.label || k) + (hasCompatBus ? '' : ' \u26a0');
        return '<option value="' + k + '"' + dis + '>' + label + '</option>';
      }).join('');

      if (board) {
        document.getElementById('be-title').textContent = t('be.edit_prefix') + board.id;
        document.getElementById('be-id').value = board.id;
        document.getElementById('be-id').disabled = true;
        typeEl.value = board.type || '';
        document.getElementById('be-del-btn').style.display = '';
      } else {
        document.getElementById('be-title').textContent = t('be.new');
        document.getElementById('be-id').value = '';
        document.getElementById('be-id').disabled = false;
        typeEl.value = guessDefaultBoardType();
        document.getElementById('be-del-btn').style.display = 'none';
      }

      beUpdateFields(board);
      beStatus('', '');
      document.getElementById('be-save-btn').disabled = false;
      document.getElementById('be-overlay').style.display = 'block';
      document.getElementById('be-modal').style.display = 'flex';
      applyLang();
    }

    function beUpdateFields(board) {
      var type = document.getElementById('be-type').value;
      var def = _boardTypes[type] || {};
      var requiredBusType = def.busType || null;

      // Bus select
      var busEl = document.getElementById('be-bus');
      var buses = (_dbgCfg && _dbgCfg.buses) || {};
      var compatKeys = requiredBusType
        ? Object.keys(buses).filter(function (k) { return buses[k].type === requiredBusType; })
        : [];

      if (!requiredBusType) {
        // MCU board — no bus required: show disabled select with single "none" option
        busEl.innerHTML = '<option value="">' + t('be.bus_none') + '</option>';
        busEl.disabled = true;
        beStatus('', '');
      } else if (compatKeys.length === 0) {
        // Bus required but none configured yet
        busEl.innerHTML = '<option value="">' + t('be.bus_missing').replace('{{type}}', requiredBusType) + '</option>';
        busEl.disabled = true;
        beStatus(t('be.no_bus_warn').replace('{{type}}', requiredBusType), 'warn');
      } else {
        // Compatible buses exist — show only them (bus is required, so no "none" option)
        busEl.innerHTML = compatKeys.map(function (k) {
          return '<option value="' + k + '">' + k + ' (' + (buses[k].type || '') + ')</option>';
        }).join('');
        busEl.disabled = false;
        // Restore selection when editing; otherwise auto-select first
        if (board && board.bus && compatKeys.indexOf(board.bus) >= 0) {
          busEl.value = board.bus;
        } else {
          busEl.value = compatKeys[0];
        }
        beStatus('', '');
      }

      // pin_count: only for SPI boards
      var isSpi = requiredBusType === 'spi_master_only';
      var pcField = document.getElementById('be-pincount-field');
      pcField.style.display = isSpi ? '' : 'none';
      if (isSpi && board && board.pin_count) {
        document.getElementById('be-pincount').value = board.pin_count;
      } else if (!board || !isSpi) {
        document.getElementById('be-pincount').value = '';
      }
    }

    function closeBoardEditor() {
      document.getElementById('be-overlay').style.display = 'none';
      document.getElementById('be-modal').style.display = 'none';
    }

    function beStatus(msg, cls) {
      var el = document.getElementById('be-status');
      el.style.display = msg ? '' : 'none';
      el.className = 'de-status ' + (cls || '');
      el.textContent = msg;
    }

    function saveBoardEditor() {
      var id = document.getElementById('be-id').disabled
        ? document.getElementById('be-id').value
        : (document.getElementById('be-id').value || '').trim();
      var type = document.getElementById('be-type').value;
      var bus = document.getElementById('be-bus').value;
      var pc = parseInt(document.getElementById('be-pincount').value, 10);

      if (!id) { beStatus(t('be.err_id'), 'err'); return; }
      if (!type) { beStatus(t('be.err_type'), 'err'); return; }
      var def = _boardTypes[type] || {};
      if (def.busType && !bus) { beStatus(t('be.err_bus_required').replace('{{type}}', def.busType), 'err'); return; }

      var entry = { id: id, type: type };
      if (bus) entry.bus = bus;
      if (def.busType === 'spi_master_only' && pc > 0) entry.pin_count = pc;

      document.getElementById('be-save-btn').disabled = true;
      beStatus(t('de.saving'), 'ok');

      fetch('/api/config')
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function (cfg) {
          if (!cfg.boards) cfg.boards = [];
          if (_beEditIdx >= 0) {
            cfg.boards[_beEditIdx] = entry;
          } else {
            for (var i = 0; i < cfg.boards.length; i++) {
              if (cfg.boards[i].id === id) {
                beStatus(t('be.err_dup'), 'err');
                document.getElementById('be-save-btn').disabled = false;
                return Promise.reject(null);
              }
            }
            cfg.boards.push(entry);
          }
          return fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg)
          });
        })
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function () { markDirty(); closeBoardEditor(); loadDebug(); })
        .catch(function (e) {
          if (e) { beStatus(t('de.err_prefix') + e.message, 'err'); document.getElementById('be-save-btn').disabled = false; }
        });
    }

    function deleteBoard(id) {
      if (!confirm(t('be.del_confirm').replace('{{id}}', id))) return;
      fetch('/api/config')
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function (cfg) {
          cfg.boards = cfg.boards.filter(function (b) { return b.id !== id; });
          cfg.devices = cfg.devices.filter(function (d) { return d.board !== id; });
          return fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg)
          });
        })
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function () { markDirty(); loadDebug(); })
        .catch(function (e) { alert(t('de.err_prefix') + e.message); });
    }

    function deleteBoardEditor() {
      closeBoardEditor();
      deleteBoard(document.getElementById('be-id').value);
    }

    /* ── Bus editor ──────────────────────────────────────────────────────── */

    var _bueEditKey = null; // bus key being edited, null = new

    // Field descriptors per bus type: { id, label, min, max, placeholder }
    var BUS_FIELDS = {
      'spi_master_only': [
        { id: 'bue-mosi',  label: 'MOSI (GPIO)', min: 0, max: 39, placeholder: '23' },
        { id: 'bue-sclk',  label: 'SCLK (GPIO)', min: 0, max: 39, placeholder: '18' },
        { id: 'bue-latch', label: 'LATCH (GPIO)', min: 0, max: 39, placeholder: '5'  }
      ],
      'uart': [
        { id: 'bue-tx',   label: 'TX (GPIO)',  min: 0,   max: 39,     placeholder: '17'     },
        { id: 'bue-rx',   label: 'RX (GPIO)',  min: 0,   max: 39,     placeholder: '16'     },
        { id: 'bue-baud', label: 'Baud',       min: 300, max: 921600, placeholder: '115200' }
      ],
      'i2c': [
        { id: 'bue-sda', label: 'SDA (GPIO)', min: 0, max: 39, placeholder: '21' },
        { id: 'bue-scl', label: 'SCL (GPIO)', min: 0, max: 39, placeholder: '22' }
      ],
      'dcc': [
        { id: 'bue-pin', label: 'Pin (GPIO)', min: 0, max: 39, placeholder: '34' }
      ]
    };

    // Map bus field id suffix to JSON key (strip 'bue-' prefix)
    function _bueFieldKey(id) { return id.replace('bue-', ''); }

    function renderBusesTab() {
      var el = document.getElementById('buses-list');
      if (!el) return;
      var cfg = _dbgCfg;
      if (!cfg) { el.innerHTML = '<div class="prm-info">' + t('prm.loading') + '</div>'; return; }

      var html = '';

      // ── Bus structurels (lecture seule, déduits de sys_pins + features) ──
      var sp   = (_dbgStatus && _dbgStatus.sys_pins)  || {};
      var feat = (_dbgStatus && _dbgStatus.features)  || {};

      function busTitle(key, type, structural) {
        return '<div class="bus-card-title">'
          + key
          + '<span class="bus-type-badge">' + type + '</span>'
          + (structural ? '<span class="bus-structural-badge">' + t('bue.structural') + '</span>' : '')
          + '</div>';
      }
      function busRow(lbl, val) {
        return '<div class="bus-row"><span class="bus-lbl">' + lbl + '</span><span class="bus-val">' + (val !== undefined && val !== null ? val : '—') + '</span></div>';
      }

      if (feat.oled) {
        var sdaPin = null, sclPin = null;
        Object.keys(sp).forEach(function (g) {
          if (sp[g] === 'SDA') sdaPin = g;
          if (sp[g] === 'SCL') sclPin = g;
        });
        html += '<div class="bus-card bus-structural">'
          + busTitle('i2c (OLED)', 'i2c', true)
          + busRow('SDA', sdaPin)
          + busRow('SCL', sclPin)
          + '</div>';
      }

      if (sp['1'] === 'TX0' || sp['3'] === 'RX0') {
        html += '<div class="bus-card bus-structural">'
          + busTitle('uart0 (debug)', 'uart', true)
          + busRow('TX', 1)
          + busRow('RX', 3)
          + '</div>';
      }

      if (feat.dcc) {
        var dccPin = null;
        Object.keys(sp).forEach(function (g) { if (sp[g] === 'DCC') dccPin = g; });
        html += '<div class="bus-card bus-structural">'
          + busTitle('dcc', 'dcc', true)
          + busRow('PIN', dccPin)
          + '</div>';
      }

      // ── Bus applicatifs (config.json) ──
      var buses = cfg.buses || {};
      var keys  = Object.keys(buses);

      if (html === '' && keys.length === 0) {
        el.innerHTML = '<div class="prm-info">' + t('bue.no_buses') + '</div>';
        return;
      }

      html += keys.map(function (k) {
        var bus = buses[k];
        var fields = BUS_FIELDS[bus.type] || [];
        var rows = fields.map(function (f) {
          return busRow(f.label.split(' ')[0], bus[_bueFieldKey(f.id)]);
        }).join('');
        var ks = k.replace(/'/g, "\\'");
        return '<div class="bus-card">'
          + busTitle(k, bus.type || '?', false)
          + rows
          + '<div class="bus-card-actions">'
          + '<button class="dbg-hbtn" onclick="openBusEditor(\'' + ks + '\')">' + t('bue.edit') + '</button>'
          + '<button class="dbg-hbtn off" onclick="deleteBus(\'' + ks + '\')">' + t('de.del') + '</button>'
          + '</div>'
          + '</div>';
      }).join('');

      el.innerHTML = html;
    }

    function openBusEditor(key) {
      _bueEditKey = key || null;
      var busData = (key && _dbgCfg && _dbgCfg.buses && _dbgCfg.buses[key]) || null;
      var feat = (_dbgStatus && _dbgStatus.features) || {};

      // Available types filtered by compiled-in features
      // DCC is always structural (#define DCC_PIN) — not configurable here
      var availTypes = ['i2c', 'uart'];
      if (feat.spi) availTypes.push('spi_master_only');

      var typeEl = document.getElementById('bue-type');
      typeEl.innerHTML = availTypes.map(function (tp) {
        return '<option value="' + tp + '">' + tp + '</option>';
      }).join('');

      if (key && busData) {
        document.getElementById('bue-title').textContent = t('bue.edit_prefix') + key;
        document.getElementById('bue-key').value = key;
        document.getElementById('bue-key').disabled = true;
        typeEl.value = busData.type || availTypes[0];
        document.getElementById('bue-del-btn').style.display = '';
      } else {
        document.getElementById('bue-title').textContent = t('bue.new');
        document.getElementById('bue-key').value = '';
        document.getElementById('bue-key').disabled = false;
        typeEl.value = availTypes[0];
        busData = null;
        document.getElementById('bue-del-btn').style.display = 'none';
      }

      bueUpdateFields(busData);
      bueStatus('', '');
      document.getElementById('bue-save-btn').disabled = false;
      document.getElementById('bue-overlay').style.display = 'block';
      document.getElementById('bue-modal').style.display = 'flex';
      applyLang();
    }

    function bueUpdateFields(busData) {
      var type = document.getElementById('bue-type').value;
      var fields = BUS_FIELDS[type] || [];

      // Build set of GPIO pins already in use (sys_pins + existing buses, excluding the one being edited)
      var usedGpios = {};
      Object.keys((_dbgStatus && _dbgStatus.sys_pins) || {}).forEach(function (g) {
        usedGpios[parseInt(g)] = true;
      });
      Object.keys((_dbgCfg && _dbgCfg.buses) || {}).forEach(function (k) {
        if (k === _bueEditKey) return; // skip bus being edited
        var b = (_dbgCfg.buses)[k];
        (BUS_FIELDS[b.type] || []).forEach(function (f) {
          var fkey = _bueFieldKey(f.id);
          if (fkey !== 'baud' && b[fkey] !== undefined) usedGpios[parseInt(b[fkey])] = true;
        });
      });

      document.getElementById('bue-fields').innerHTML = fields.map(function (f) {
        var fkey = _bueFieldKey(f.id);
        var val;
        if (busData && busData[fkey] !== undefined) {
          val = busData[fkey]; // édition : valeur existante
        } else if (fkey === 'baud') {
          val = f.placeholder; // baud : toujours suggéré
        } else {
          var suggested = parseInt(f.placeholder);
          val = (!isNaN(suggested) && !usedGpios[suggested]) ? suggested : '';
        }
        return '<div class="de-field">'
          + '<label>' + f.label + '</label>'
          + '<input type="number" id="' + f.id + '" min="' + f.min + '" max="' + f.max + '"'
          + ' placeholder="' + f.placeholder + '" value="' + val + '">'
          + '</div>';
      }).join('');
    }

    function closeBusEditor() {
      document.getElementById('bue-overlay').style.display = 'none';
      document.getElementById('bue-modal').style.display = 'none';
    }

    function bueStatus(msg, cls) {
      var el = document.getElementById('bue-status');
      el.style.display = msg ? '' : 'none';
      el.className = 'de-status ' + (cls || '');
      el.textContent = msg;
    }

    function saveBusEditor() {
      var key = _bueEditKey || (document.getElementById('bue-key').value || '').trim();
      if (!key) { bueStatus(t('bue.err_key'), 'err'); return; }

      var type = document.getElementById('bue-type').value;
      var bus = { type: type };
      var fields = BUS_FIELDS[type] || [];
      var valid = true;
      fields.forEach(function (f) {
        if (!valid) return;
        var el = document.getElementById(f.id);
        var n = el ? parseInt(el.value, 10) : NaN;
        if (isNaN(n) || n < f.min || n > f.max) {
          bueStatus(t('bue.err_field') + f.label, 'err');
          valid = false;
          return;
        }
        bus[_bueFieldKey(f.id)] = n;
      });
      if (!valid) return;

      document.getElementById('bue-save-btn').disabled = true;
      bueStatus(t('de.saving'), 'ok');

      fetch('/api/config')
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function (cfg) {
          if (!cfg.buses) cfg.buses = {};
          if (!_bueEditKey && cfg.buses[key]) {
            bueStatus(t('bue.err_dup'), 'err');
            document.getElementById('bue-save-btn').disabled = false;
            return Promise.reject(null);
          }
          cfg.buses[key] = bus;
          return fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg)
          });
        })
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function () { markDirty(); closeBusEditor(); loadDebug(); })
        .catch(function (e) {
          if (e) { bueStatus(t('de.err_prefix') + e.message, 'err'); document.getElementById('bue-save-btn').disabled = false; }
        });
    }

    function deleteBus(key) {
      if (!confirm(t('bue.del_confirm').replace('{{key}}', key))) return;
      fetch('/api/config')
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function (cfg) {
          delete cfg.buses[key];
          // Remove boards that depended on this bus
          cfg.boards = cfg.boards.filter(function (b) { return b.bus !== key; });
          return fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg)
          });
        })
        .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
        .then(function () { markDirty(); loadDebug(); renderBusesTab(); })
        .catch(function (e) { alert(t('de.err_prefix') + e.message); });
    }

    function deleteBusEditor() {
      var key = _bueEditKey;
      closeBusEditor();
      deleteBus(key);
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

