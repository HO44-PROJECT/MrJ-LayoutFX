/**
 * @file WebUI.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#ifdef WEBUI
#ifdef ESP32

#include "api/WebUI.h"
#include "api/ApiServer.h"
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Static member
// ---------------------------------------------------------------------------

const DeviceFactory* WebUI::_factory = nullptr;

// ---------------------------------------------------------------------------
// HTML page — stored in flash (PROGMEM)
// ---------------------------------------------------------------------------

static const char WEBUI_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MrJ RailwayFX</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:'Segoe UI',Arial,sans-serif;min-height:100vh}
header{background:#16213e;padding:1rem 1.5rem;border-bottom:2px solid #0f3460;display:flex;align-items:center;justify-content:space-between}
h1{font-size:1.1rem;color:#e94560;letter-spacing:3px;text-transform:uppercase}
#sb{font-size:.75rem;color:#555}
#sb span{color:#4CAF50}
#err{display:none;background:#c0392b;color:#fff;text-align:center;padding:.4rem;font-size:.8rem}
#grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(190px,1fr));gap:1rem;padding:1.5rem}
.card{background:#16213e;border-radius:8px;border:2px solid #0f3460;padding:1.2rem;display:flex;flex-direction:column;gap:.8rem;transition:border-color .4s}
.card.on{border-color:#27ae60}
.card.off{border-color:#2c2c3e}
.card.busy{border-color:#e67e22}
.ch{display:flex;align-items:center;justify-content:space-between}
.cid{font-weight:700;font-size:.95rem;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dot{width:10px;height:10px;border-radius:50%;flex-shrink:0}
.dot.on{background:#27ae60;box-shadow:0 0 6px #27ae60}
.dot.off{background:#3d3d3d}
.dot.busy{background:#e67e22;animation:pulse .7s infinite alternate}
@keyframes pulse{from{box-shadow:0 0 2px #e67e22}to{box-shadow:0 0 10px #e67e22}}
.badge{font-size:.62rem;background:#0f3460;color:#7f8c8d;padding:2px 7px;border-radius:4px;align-self:flex-start;letter-spacing:.5px}
.btn{width:100%;padding:.5rem;border:none;border-radius:6px;cursor:pointer;font-size:.82rem;font-weight:700;letter-spacing:1px;transition:opacity .15s}
.btn:active{opacity:.65}
.btn.on{background:#c0392b;color:#fff}
.btn.off{background:#27ae60;color:#fff}
.btn.busy{background:#3d3d3d;color:#666;cursor:default}
.btn.static{background:#1e3a5f;color:#4a6fa5;cursor:default;font-style:italic}
</style>
</head>
<body>
<div id="err">Connexion perdue — reconnexion en cours…</div>
<header>
  <h1>&#9881; MrJ RailwayFX</h1>
  <div id="sb">— appareils</div>
</header>
<div id="grid"></div>
<script>
var POLL=3000;
var STATIC_TYPES=['StaticLow'];

function cls(d){
  if(STATIC_TYPES.indexOf(d.type)>=0)return'static';
  if(d.state<0)return'busy';
  if(d.desired>0)return'on';
  return'off';
}
function lbl(c){
  if(c==='static')return'FIXE';
  if(c==='busy')return'…';
  if(c==='on')return'ÉTEINDRE';
  return'ALLUMER';
}
function card(d){
  var c=cls(d);
  var dis=(c==='busy'||c==='static')?'disabled':'';
  return'<div class="card '+c+'">'
    +'<div class="ch"><span class="cid" title="'+d.id+'">'+d.id+'</span>'
    +'<span class="dot '+c+'"></span></div>'
    +'<span class="badge">'+d.type+'</span>'
    +'<button class="btn '+c+'" onclick="tog(\''+d.id+'\','+d.desired+')" '+dis+'>'+lbl(c)+'</button>'
    +'</div>';
}
function render(devs){
  document.getElementById('grid').innerHTML=devs.map(card).join('');
  var now=new Date().toLocaleTimeString('fr-FR');
  document.getElementById('sb').innerHTML='<span>'+devs.length+'</span> appareils &mdash; '+now;
}
function poll(){
  fetch('/api/devices')
    .then(function(r){if(!r.ok)throw r;return r.json();})
    .then(function(d){
      document.getElementById('err').style.display='none';
      render(d);
    })
    .catch(function(){document.getElementById('err').style.display='block';});
}
function tog(id,desired){
  var ns=desired>0?0:1;
  fetch('/api/device',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({id:id,state:ns})})
  .then(poll)
  .catch(function(){document.getElementById('err').style.display='block';});
}
poll();
setInterval(poll,POLL);
</script>
</body>
</html>)html";

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void WebUI::init(const DeviceFactory& factory) {
    _factory = &factory;

    ApiServer::on("/ui",          HTTP_GET,  _onGetUi);
    ApiServer::on("/api/devices", HTTP_GET,  _onGetDevices);
    ApiServer::on("/api/device",  HTTP_POST, _onPostDevice);
}

// ---------------------------------------------------------------------------
// Private — HTTP handlers
// ---------------------------------------------------------------------------

void WebUI::_onGetUi() {
    ApiServer::server().send_P(200, "text/html", WEBUI_HTML);
}

void WebUI::_onGetDevices() {
    String json = "[";
    for (size_t i = 0; i < _factory->count(); i++) {
        Device* d = _factory->device(i);
        if (i > 0) json += ",";
        json += F("{\"id\":\"");
        json += _factory->deviceId(i);
        json += F("\",\"type\":\"");
        json += d->getDeviceName();
        json += F("\",\"state\":");
        json += (int)d->getState();
        json += F(",\"desired\":");
        json += (int)d->getDesiredState();
        json += F("}");
    }
    json += "]";
    ApiServer::server().send(200, "application/json", json);
}

void WebUI::_onPostDevice() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }

    const char* id = doc["id"] | "";
    if (!doc["state"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"state required\"}"));
        return;
    }
    int state = doc["state"].as<int>();

    for (size_t i = 0; i < _factory->count(); i++) {
        if (strcmp(_factory->deviceId(i), id) == 0) {
            _factory->device(i)->newState((STATE_TYPE)state);
            ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
            return;
        }
    }

    ApiServer::server().send(404, "application/json", F("{\"error\":\"device not found\"}"));
}

#endif  // ESP32
#endif  // WEBUI
