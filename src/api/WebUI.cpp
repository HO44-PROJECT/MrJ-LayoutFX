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
header{background:#16213e;padding:1rem 1.5rem;border-bottom:2px solid #0f3460}
.hdr-top{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:.5rem;margin-bottom:.5rem}
h1{font-size:1.1rem;color:#e94560;letter-spacing:3px;text-transform:uppercase}
#sb{font-size:.75rem;color:#555}
#sb span{color:#4CAF50}
#err{display:none;background:#c0392b;color:#fff;text-align:center;padding:.4rem;font-size:.8rem}
.hdr-btns{display:flex;gap:.5rem}
.gbtn{padding:.35rem .8rem;border:none;border-radius:5px;cursor:pointer;font-size:.75rem;font-weight:700;letter-spacing:1px;transition:opacity .15s}
.gbtn:active{opacity:.6}
.gbtn.on{background:#27ae60;color:#fff}
.gbtn.off{background:#c0392b;color:#fff}
#grid{padding:1.5rem;display:flex;flex-direction:column;gap:1.8rem}
.ghdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:.8rem;border-bottom:1px solid #0f3460;padding-bottom:.5rem}
.gname{font-size:.85rem;font-weight:700;color:#7f8c8d;letter-spacing:2px;text-transform:uppercase}
.gcnt{color:#4a6fa5;font-size:.8rem;font-weight:400}
.gbtns{display:flex;gap:.4rem}
.gcards{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:1rem}
.card{background:#16213e;border-radius:8px;border:2px solid #0f3460;padding:1rem;display:flex;flex-direction:column;gap:.6rem;transition:border-color .4s}
.card.on{border-color:#27ae60}
.card.off{border-color:#2c2c3e}
.card.busy{border-color:#e67e22}
.ch{display:flex;align-items:center;justify-content:space-between}
.cid{font-weight:700;font-size:.85rem;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dot{width:9px;height:9px;border-radius:50%;flex-shrink:0}
.dot.on{background:#27ae60;box-shadow:0 0 6px #27ae60}
.dot.off{background:#3d3d3d}
.dot.busy{background:#e67e22;animation:pulse .7s infinite alternate}
@keyframes pulse{from{box-shadow:0 0 2px #e67e22}to{box-shadow:0 0 10px #e67e22}}
.icon{height:44px;display:flex;align-items:center;justify-content:center;color:#3a5070}
.icon svg{width:40px;height:40px;transition:color .4s}
.card.on .icon{color:#4CAF50}
.card.busy .icon{color:#e67e22}
.badge{font-size:.6rem;background:#0f3460;color:#7f8c8d;padding:2px 6px;border-radius:4px;align-self:flex-start;letter-spacing:.5px}
.meta{display:flex;flex-wrap:wrap;gap:3px}
.mtag{font-size:.58rem;background:#0d1f38;color:#4a6fa5;padding:1px 5px;border-radius:3px;letter-spacing:.3px}
.btn{width:100%;padding:.45rem;border:none;border-radius:6px;cursor:pointer;font-size:.78rem;font-weight:700;letter-spacing:1px;transition:opacity .15s}
.btn:active{opacity:.65}
.btn.on{background:#c0392b;color:#fff}
.btn.off{background:#27ae60;color:#fff}
.btn.busy{background:#3d3d3d;color:#666;cursor:default}
.btn.static{background:#1e3a5f;color:#4a6fa5;cursor:default;font-style:italic}
.card.stop{border-color:#c0392b}
.dot.stop{background:#c0392b;box-shadow:0 0 6px #c0392b}
.card.flash{border-color:#e67e22}
.dot.flash{background:#e67e22;box-shadow:0 0 6px #e67e22}
.tbtns{display:flex;gap:3px}
.tbtn{flex:1;padding:.35rem .05rem;border:none;border-radius:5px;cursor:pointer;font-size:.64rem;font-weight:700;letter-spacing:.3px;opacity:.28;transition:opacity .15s}
.tbtn.active{opacity:1}
.tbtn:not(.active):not([disabled]):hover{opacity:.6}
.tbtn[disabled]{cursor:default}
.tbtn.t-off{background:#2c2c3e;color:#aaa}
.tbtn.t-stop{background:#c0392b;color:#fff}
.tbtn.t-go{background:#27ae60;color:#fff}
.tbtn.t-flash{background:#e67e22;color:#fff}
</style>
</head>
<body>
<div id="err">Connexion perdue — reconnexion en cours…</div>
<header>
  <div class="hdr-top">
    <h1>&#9881; MrJ RailwayFX</h1>
    <div class="hdr-btns">
      <button class="gbtn on" onclick="allDevices(1)">TOUT ALLUMER</button>
      <button class="gbtn off" onclick="allDevices(0)">TOUT ÉTEINDRE</button>
    </div>
  </div>
  <div id="sb">— appareils</div>
</header>
<div id="grid"></div>
<script>
var POLL=3000;
var STATIC_TYPES=['StaticLow'];
var TRAFFIC_TYPES=['Traffic Light 3ph','Traffic Light 4ph'];

/* ── Icônes SVG par type ───────────────────────────────────────── */
var S='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5">';
var E='</svg>';
var ICONS={
  'Beacon':S
    +'<circle cx="12" cy="10" r="3.5"/>'
    +'<path d="M12 1v3M12 16v3M3 10H1M23 10h-2M4.2 3.8l2 2M16.8 16.2l2 2M4.2 16.2l2-2M16.8 3.8l2-2"/>'
    +'<line x1="8" y1="21" x2="16" y2="21"/>'
    +E,

  'DoubleBeacon':S
    +'<circle cx="7" cy="9" r="2.5"/>'
    +'<path d="M7 2v2M7 14v2M1 9H3M11 9h2M3.5 5.5l1.5 1.5M8.5 12.5l1.5 1.5M3.5 12.5l1.5-1.5M8.5 5.5l1.5-1.5"/>'
    +'<circle cx="17" cy="9" r="2.5"/>'
    +'<path d="M17 2v2M17 14v2M13 9h2M21 9h2M13.5 5.5l1.5 1.5M18.5 12.5l1.5 1.5M13.5 12.5l1.5-1.5M18.5 5.5l1.5-1.5"/>'
    +'<line x1="5" y1="20" x2="19" y2="20"/>'
    +E,

  'Gas Lamp':S
    +'<line x1="8" y1="22" x2="16" y2="22"/>'
    +'<line x1="10" y1="20" x2="14" y2="20"/>'
    +'<line x1="12" y1="20" x2="12" y2="11"/>'
    +'<rect x="9" y="8" width="6" height="6" rx="0.5"/>'
    +'<polyline points="9,8 12,4 15,8"/>'
    +'<line x1="12" y1="4" x2="12" y2="2"/>'
    +E,

  'Electric Lamp':S
    +'<path d="M10 21h4M11 21v-2M13 21v-2"/>'
    +'<path d="M12 3a6 6 0 0 1 4 10.5V17a1 1 0 0 1-1 1H9a1 1 0 0 1-1-1v-3.5A6 6 0 0 1 12 3Z"/>'
    +'<line x1="10" y1="13" x2="10" y2="15"/>'
    +E,

  'CampFire':S
    +'<path d="M12 2C11 5 9 8 11 11C11.5 12.5 12 13 12 13C12 13 12.5 12.5 13 11C15 8 13 5 12 2Z"/>'
    +'<path d="M9.5 13C8.5 15 9.5 17.5 12 18C14.5 17.5 15.5 15 14.5 13"/>'
    +'<line x1="5" y1="22" x2="19" y2="22"/>'
    +'<line x1="6" y1="18" x2="9.5" y2="22"/>'
    +'<line x1="18" y1="18" x2="14.5" y2="22"/>'
    +E,

  'Torch':S
    +'<line x1="6" y1="22" x2="14" y2="13"/>'
    +'<line x1="12" y1="14" x2="16" y2="12"/>'
    +'<path d="M13 13 Q10 9 13 5 Q15 8 16 6 Q18 10 16 13Z"/>'
    +E,

  'Turn Signal':S
    +'<path d="M4 12L13 3V8H19V16H13V21Z"/>'
    +E,

  'Storm':S
    +'<path d="M20 9a8 8 0 0 0-15.3-2.2A4.5 4.5 0 0 0 5 16h13a3 3 0 0 0 2-5.2"/>'
    +'<polyline points="12,11 10,16 14,16 12,21"/>'
    +E,

  'SolderLamp':S
    +'<line x1="3" y1="21" x2="11" y2="13"/>'
    +'<line x1="13" y1="11" x2="21" y2="3"/>'
    +'<circle cx="12" cy="12" r="2"/>'
    +'<path d="M9 9L7 7M11 7L9 5M7 9L5 11" stroke-width="1.2"/>'
    +E,

  'DefectLamp':S
    +'<path d="M10 21h4M11 21v-2M13 21v-2"/>'
    +'<path d="M12 3a6 6 0 0 1 4 10.5V17a1 1 0 0 1-1 1H9a1 1 0 0 1-1-1v-3.5A6 6 0 0 1 12 3Z"/>'
    +'<line x1="9.5" y1="7.5" x2="14.5" y2="14.5"/>'
    +'<line x1="14.5" y1="7.5" x2="9.5" y2="14.5"/>'
    +E,

  'Neon Sign':S
    +'<rect x="2" y="4" width="20" height="16" rx="2"/>'
    +'<rect x="5" y="7" width="14" height="10" rx="1" stroke-width="1"/>'
    +'<path d="M6 15L6 9L9 12.5L12 9L12 15"/>'
    +'<path d="M13 15v-4M13 12q1.5-1.5 2.5 0"/>'
    +'<path d="M16.5 9h3M18 9v5q0 1.5-1.5 1.5"/>'
    +E,

  'OilLamp':S
    +'<path d="M3 14Q3 19 12 19Q21 19 21 14L18 12Q15 11 12 11Q9 11 6 12Z"/>'
    +'<line x1="12" y1="11" x2="12" y2="7"/>'
    +'<path d="M12 7Q14 4 13 2Q12 4 11 2Q10 4 12 7"/>'
    +'<path d="M18 12Q22 12 22 9Q22 6 18 7"/>'
    +E,

  'SignalFlare':S
    +'<line x1="4" y1="20" x2="20" y2="4"/>'
    +'<path d="M17 4h4v4"/>'
    +'<path d="M14 7L12 5M16 7L18 5M14 5L14 3" stroke-width="1.2"/>'
    +E,

  'Train Head Lamp':S
    +'<rect x="3" y="7" width="18" height="12" rx="3"/>'
    +'<circle cx="8.5" cy="13" r="2.5"/>'
    +'<circle cx="15.5" cy="13" r="2.5"/>'
    +'<line x1="3" y1="10" x2="1" y2="10"/>'
    +'<line x1="3" y1="16" x2="1" y2="16"/>'
    +'<line x1="8" y1="19" x2="7" y2="22"/>'
    +'<line x1="16" y1="19" x2="17" y2="22"/>'
    +E,

  'Railway Crossing Lights':S
    +'<line x1="12" y1="2" x2="12" y2="22"/>'
    +'<line x1="3" y1="7" x2="21" y2="17"/>'
    +'<line x1="21" y1="7" x2="3" y2="17"/>'
    +'<circle cx="3" cy="7" r="2.5" fill="currentColor" stroke="none"/>'
    +'<circle cx="21" cy="17" r="2.5" fill="currentColor" stroke="none"/>'
    +E,

  'StaticLow':S
    +'<line x1="4" y1="10" x2="20" y2="10"/>'
    +'<line x1="7" y1="14" x2="17" y2="14"/>'
    +'<line x1="10" y1="18" x2="14" y2="18"/>'
    +E,

  'DB Bloc Signal':S
    +'<line x1="12" y1="22" x2="12" y2="4"/>'
    +'<circle cx="12" cy="7" r="2.5"/>'
    +'<circle cx="12" cy="15" r="2.5"/>'
    +'<line x1="12" y1="9" x2="19" y2="6"/>'
    +E,

  'DB Entry Signal':S
    +'<line x1="12" y1="22" x2="12" y2="2"/>'
    +'<circle cx="12" cy="5" r="2"/>'
    +'<circle cx="12" cy="11" r="2"/>'
    +'<circle cx="12" cy="17" r="2"/>'
    +'<line x1="12" y1="13" x2="19" y2="10"/>'
    +E,

  'DB Exit Signal':S
    +'<line x1="12" y1="22" x2="12" y2="2"/>'
    +'<circle cx="12" cy="5" r="2"/>'
    +'<circle cx="12" cy="11" r="2"/>'
    +'<circle cx="12" cy="17" r="2"/>'
    +'<line x1="12" y1="13" x2="5" y2="10"/>'
    +E,

  'Traffic Light 3ph':S
    +'<rect x="7" y="2" width="10" height="17" rx="2"/>'
    +'<circle cx="12" cy="6" r="2"/>'
    +'<circle cx="12" cy="11" r="2"/>'
    +'<circle cx="12" cy="16" r="2"/>'
    +'<line x1="12" y1="19" x2="12" y2="22"/>'
    +E,

  'Traffic Light 4ph':S
    +'<rect x="7" y="1" width="10" height="20" rx="2"/>'
    +'<circle cx="12" cy="5" r="1.7"/>'
    +'<circle cx="12" cy="10" r="1.7"/>'
    +'<circle cx="12" cy="15" r="1.7"/>'
    +'<circle cx="12" cy="20" r="1.7"/>'
    +E,

  'DfAudio':S
    +'<polygon points="11,5 6,9 2,9 2,15 6,15 11,19"/>'
    +'<path d="M15.5 8.5a5 5 0 0 1 0 7"/>'
    +'<path d="M19 5a10 10 0 0 1 0 14"/>'
    +E,

  'Servo':S
    +'<circle cx="12" cy="12" r="5.5"/>'
    +'<circle cx="12" cy="12" r="2"/>'
    +'<path d="M12 1v3M12 20v3M1 12h3M20 12h3M4.2 4.2l2 2M16.8 16.8l2 2M19.8 4.2l-2 2M7.2 16.8l-2 2"/>'
    +E,

  '_':S
    +'<circle cx="12" cy="12" r="9"/>'
    +'<path d="M9.5 9.5a3 3 0 1 1 3 3V14"/>'
    +'<circle cx="12" cy="17" r=".7" fill="currentColor" stroke="none"/>'
    +E
};

function cls(d){
  if(STATIC_TYPES.indexOf(d.type)>=0)return'static';
  if(d.state<0)return'busy';
  if(d.desired>0)return'on';
  return'off';
}
function clsTraffic(d){
  if(d.state<0)return'busy';
  if(d.desired===2)return'on';
  if(d.desired===1)return'stop';
  if(d.desired===3)return'flash';
  return'off';
}
function lbl(c){
  if(c==='static')return'FIXE';
  if(c==='busy')return'…';
  if(c==='on')return'ÉTEINDRE';
  return'ALLUMER';
}
function meta(d){
  var t='<div class="meta">';
  if(d.addr>0) t+='<span class="mtag">DCC '+d.addr+'</span>';
  if(d.board>0) t+='<span class="mtag">Carte '+d.board+' · pin '+d.pin+'</span>';
  else if(d.pin>0||d.board===0) t+='<span class="mtag">GPIO '+d.pin+'</span>';
  return t+'</div>';
}
function cardTraffic(d){
  var c=clsTraffic(d);
  var busy=d.state<0;
  var dis=busy?'disabled':'';
  var ico=ICONS[d.type]||ICONS['_'];
  function tbtn(lbl,cls,st){
    var act=(d.desired===st&&!busy)?'active':'';
    return'<button class="tbtn t-'+cls+' '+act+'" onclick="setTraffic(\''+d.id+'\','+st+')" '+dis+'>'+lbl+'</button>';
  }
  return'<div class="card '+c+'">'
    +'<div class="ch"><span class="cid" title="'+d.id+'">'+d.id+'</span>'
    +'<span class="dot '+c+'"></span></div>'
    +'<div class="icon">'+ico+'</div>'
    +'<span class="badge">'+d.type+'</span>'
    +meta(d)
    +'<div class="tbtns">'+tbtn('OFF','off',0)+tbtn('GO','go',2)+tbtn('FLASH','flash',3)+tbtn('STOP','stop',1)+'</div>'
    +'</div>';
}
function card(d){
  if(TRAFFIC_TYPES.indexOf(d.type)>=0)return cardTraffic(d);
  var c=cls(d);
  var dis=(c==='busy'||c==='static')?'disabled':'';
  var ico=ICONS[d.type]||ICONS['_'];
  return'<div class="card '+c+'">'
    +'<div class="ch"><span class="cid" title="'+d.id+'">'+d.id+'</span>'
    +'<span class="dot '+c+'"></span></div>'
    +'<div class="icon">'+ico+'</div>'
    +'<span class="badge">'+d.type+'</span>'
    +meta(d)
    +'<button class="btn '+c+'" onclick="tog(\''+d.id+'\','+d.desired+')" '+dis+'>'+lbl(c)+'</button>'
    +'</div>';
}
function render(devs){
  var order=[];
  var groups={};
  devs.forEach(function(d){
    if(!groups[d.type]){groups[d.type]=[];order.push(d.type);}
    groups[d.type].push(d);
  });
  var html='';
  order.forEach(function(type){
    var list=groups[type];
    var isStatic=STATIC_TYPES.indexOf(type)>=0;
    html+='<div class="group">';
    html+='<div class="ghdr"><span class="gname">'+type+' <span class="gcnt">('+list.length+')</span></span>';
    if(!isStatic){
      html+='<div class="gbtns">'
        +'<button class="gbtn on" onclick="groupDevices(\''+type+'\',1)">ALLUMER</button>'
        +'<button class="gbtn off" onclick="groupDevices(\''+type+'\',0)">ÉTEINDRE</button>'
        +'</div>';
    }
    html+='</div>';
    html+='<div class="gcards">'+list.map(card).join('')+'</div>';
    html+='</div>';
  });
  document.getElementById('grid').innerHTML=html;
  var now=new Date().toLocaleTimeString('fr-FR');
  document.getElementById('sb').innerHTML='<span>'+devs.length+'</span> appareils &mdash; '+now;
}
function post(url,body){
  return fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
}
function showErr(){document.getElementById('err').style.display='block';}
function poll(){
  fetch('/api/devices')
    .then(function(r){if(!r.ok)throw r;return r.json();})
    .then(function(d){document.getElementById('err').style.display='none';render(d);})
    .catch(showErr);
}
function tog(id,desired){
  post('/api/device',{id:id,state:desired>0?0:1}).then(poll).catch(showErr);
}
function setTraffic(id,state){
  post('/api/device',{id:id,state:state}).then(poll).catch(showErr);
}
function allDevices(state){
  post('/api/all',{state:state}).then(poll).catch(showErr);
}
function groupDevices(type,state){
  post('/api/group',{type:type,state:state}).then(poll).catch(showErr);
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
    ApiServer::on("/api/all",     HTTP_POST, _onAllDevices);
    ApiServer::on("/api/group",   HTTP_POST, _onGroupDevices);
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
        Device*  d     = _factory->device(i);
        uint8_t  board = _factory->deviceBoard(i);
        uint8_t  pin   = 0;
        if (d->getPinCount() > 0) {
#ifdef SPI_CARDS
            pin = d->getPin(0).pin;
#else
            pin = (uint8_t)d->getPin(0);
#endif
        }
        if (i > 0) json += ",";
        json += F("{\"id\":\"");
        json += _factory->deviceId(i);
        json += F("\",\"type\":\"");
        json += d->getDeviceName();
        json += F("\",\"state\":");
        json += (int)d->getState();
        json += F(",\"desired\":");
        json += (int)d->getDesiredState();
        json += F(",\"addr\":");
        json += (int)d->getDccAddress();
        json += F(",\"board\":");
        json += (int)board;
        json += F(",\"pin\":");
        json += (int)pin;
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

    Serial.print(F("WebUI: device \""));
    Serial.print(id);
    Serial.print(F("\" → state "));
    Serial.println(state);

    for (size_t i = 0; i < _factory->count(); i++) {
        if (strcmp(_factory->deviceId(i), id) == 0) {
            _factory->device(i)->newState((STATE_TYPE)state);
            ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
            return;
        }
    }

    Serial.print(F("WebUI: device not found: "));
    Serial.println(id);
    ApiServer::server().send(404, "application/json", F("{\"error\":\"device not found\"}"));
}

void WebUI::_onAllDevices() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err || !doc["state"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }
    int state = doc["state"].as<int>();

    Serial.print(F("WebUI: all → state "));
    Serial.println(state);

    for (size_t i = 0; i < _factory->count(); i++) {
        Device* d = _factory->device(i);
        if (strcmp("StaticLow", (const char*)d->getDeviceName()) != 0) {
            d->newState((STATE_TYPE)state);
        }
    }
    ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

void WebUI::_onGroupDevices() {
    if (!ApiServer::server().hasArg("plain")) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"body required\"}"));
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ApiServer::server().arg("plain"));
    if (err || !doc["state"].is<int>()) {
        ApiServer::server().send(400, "application/json", F("{\"error\":\"invalid JSON\"}"));
        return;
    }
    int state = doc["state"].as<int>();
    const char* type = doc["type"] | "";

    Serial.print(F("WebUI: group \""));
    Serial.print(type);
    Serial.print(F("\" → state "));
    Serial.println(state);

    for (size_t i = 0; i < _factory->count(); i++) {
        Device* d = _factory->device(i);
        if (strcmp(type, (const char*)d->getDeviceName()) == 0) {
            d->newState((STATE_TYPE)state);
        }
    }
    ApiServer::server().send(200, "application/json", F("{\"ok\":true}"));
}

#endif  // ESP32
#endif  // WEBUI
