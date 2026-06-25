/**
 * @file OtaUpdater.cpp
 * @brief Implementation of ArduinoOTA (espota) + web /update firmware updates.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/OtaUpdater.h"

#ifdef MRJFX_OTA_ENABLED

  #include <Arduino.h>
  #include <ArduinoOTA.h> // pulls in ESPmDNS + Update
  #include <Update.h>

  #ifdef MRJFX_API_SERVER_ENABLED
    #include "api/ApiServer.h"
    #include <WebServer.h>
  #endif

  #ifdef MRJFX_OLED_ENABLED
    #include "oled/OledDisplay.h"
  #endif

namespace {
char _host[24] = {0};

// "<OTA_HOSTNAME>-<2 MAC bytes>" → stable per-device name, avoids mDNS clashes.
void _buildHostname() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(_host, sizeof(_host), "%s-%04x", OTA_HOSTNAME,
           (unsigned)((mac >> 32) & 0xFFFF));
}
} // namespace

namespace OtaUpdater {

const char *hostname() { return _host; }

void beginArduinoOta() {
  _buildHostname();
  ArduinoOTA.setHostname(_host);
  #ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(OTA_PASSWORD);
  #endif

  ArduinoOTA
      .onStart([]() {
        Serial.println(F("[OTA] update started (espota)"));
  #ifdef MRJFX_OLED_ENABLED
        OledDisplay::log("OTA update...");
  #endif
      })
      .onEnd([]() { Serial.println(F("[OTA] done — rebooting")); })
      .onProgress([](unsigned p, unsigned t) {
        Serial.printf("[OTA] %u%%\r", t ? (p * 100u / t) : 0u);
      })
      .onError([](ota_error_t e) { Serial.printf("[OTA] error %u\n", (unsigned)e); });

  ArduinoOTA.begin();
  Serial.print(F("[OTA] ready — host "));
  Serial.print(_host);
  Serial.println(F(".local  (espota :3232 / web /update)"));
}

void handle() { ArduinoOTA.handle(); }

  #ifdef MRJFX_API_SERVER_ENABLED

namespace {

// Minimal self-contained upload page (no WebUI rebuild needed; works in AP mode).
const char _FORM[] PROGMEM =
    "<!doctype html><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>MrJFX OTA</title>"
    "<style>body{font-family:sans-serif;max-width:30em;margin:2em auto;padding:0 1em}"
    "progress{width:100%}"
    ".btn{display:inline-block;padding:.5em 1em;border:1px solid #888;border-radius:6px;"
    "background:#f4f4f4;cursor:pointer}.btn:hover{background:#e8e8e8}"
    "#fn{margin-left:.6em;color:#555;font-size:.9em}"
    ".hint{font-size:.8em;color:#777;margin:.4em 0 1em}</style>"
    "<h2>Mise à jour firmware</h2>"
    "<form id=f>"
    "<p><label class=btn for=fw>Choisir un fichier .bin</label>"
    "<span id=fn>Aucun fichier sélectionné</span>"
    "<input id=fw type=file name=firmware accept=.bin required hidden>"
    "<p class=hint>Fichier : .pio/build/&lt;env&gt;/firmware.bin</p>"
    "<p><button id=go disabled>Envoyer</button></form>"
    "<progress id=p value=0 max=100 hidden></progress><pre id=o></pre>"
    "<script>"
    "fw.onchange=function(){var n=fw.files[0];fn.textContent=n?n.name:'Aucun fichier sélectionné';"
    "go.disabled=!n};"
    "f.onsubmit=function(e){e.preventDefault();"
    "var fd=new FormData(f),x=new XMLHttpRequest();p.hidden=false;o.textContent='';go.disabled=true;"
    "x.upload.onprogress=function(ev){if(ev.lengthComputable)p.value=ev.loaded/ev.total*100};"
    "x.onload=function(){o.textContent=x.responseText;"
    "if(x.status==200)setTimeout(function(){location='/ui'},7000)};"
    "x.onerror=function(){o.textContent='Erreur réseau'};"
    "x.open('POST','/update');x.send(fd)};"
    "</script>";

bool _denied = false; // set when an upload fails auth, to refuse the write/restart

// Basic-auth gate, only when OTA_PASSWORD is set. Sends 401 on failure.
bool _authOk(WebServer &s) {
  #ifdef OTA_PASSWORD
  if (!s.authenticate(OTA_HOSTNAME, OTA_PASSWORD)) {
    s.requestAuthentication();
    return false;
  }
  #else
  (void)s;
  #endif
  return true;
}

} // namespace

void registerWebRoutes() {
  WebServer &srv = ApiServer::server();

  // Upload form.
  srv.on("/update", HTTP_GET, []() {
    WebServer &s = ApiServer::server();
    if (!_authOk(s)) return;
    s.send_P(200, "text/html", _FORM);
  });

  // Firmware upload: completion handler + streamed upload handler.
  srv.on(
      "/update", HTTP_POST,
      []() {
        WebServer &s = ApiServer::server();
        if (_denied) { s.requestAuthentication(); return; }
        bool ok = !Update.hasError();
        s.sendHeader("Connection", "close");
        s.send(200, "text/plain", ok ? "OK — redemarrage" : "ECHEC — voir le log serie");
        if (ok) {
          delay(500);
          ESP.restart();
        }
      },
      []() {
        WebServer &s = ApiServer::server();
        HTTPUpload &up = s.upload();
        if (up.status == UPLOAD_FILE_START) {
          _denied = false;
  #ifdef OTA_PASSWORD
          if (!s.authenticate(OTA_HOSTNAME, OTA_PASSWORD)) { _denied = true; return; }
  #endif
          Serial.printf("[OTA] web upload: %s\n", up.filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
        } else if (up.status == UPLOAD_FILE_WRITE) {
          if (_denied) return;
          if (Update.write(up.buf, up.currentSize) != up.currentSize)
            Update.printError(Serial);
        } else if (up.status == UPLOAD_FILE_END) {
          if (_denied) return;
          if (Update.end(true)) Serial.printf("[OTA] web OK: %u bytes\n", (unsigned)up.totalSize);
          else Update.printError(Serial);
        }
      });
}

  #endif // MRJFX_API_SERVER_ENABLED

} // namespace OtaUpdater

#endif // MRJFX_OTA_ENABLED
