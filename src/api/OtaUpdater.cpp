/**
 * @file OtaUpdater.cpp
 * @brief Implementation of ArduinoOTA (espota) + web /update firmware updates.
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/OtaUpdater.h"

#ifdef LFX_OTA_ENABLED

  #include <Arduino.h>
  #include <ArduinoOTA.h> // pulls in ESPmDNS + Update
  #include <Update.h>

  #ifdef LFX_API_SERVER_ENABLED
    #include "api/ApiServer.h"
    #include <WebServer.h>
  #endif

  #ifdef LFX_OLED_ENABLED
    #include "oled/OledDisplay.h"
  #endif

namespace {
char _host[24] = {0};
bool _inProgress = false; // set on transfer start, never cleared (device reboots on completion)

// "<OTA_HOSTNAME>-<2 MAC bytes>" → stable per-device name, avoids mDNS clashes.
void _buildHostname() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(_host, sizeof(_host), "%s-%04x", OTA_HOSTNAME,
           (unsigned)((mac >> 32) & 0xFFFF));
}
} // namespace

namespace OtaUpdater {

const char *hostname() { return _host; }
bool inProgress() { return _inProgress; }

void beginArduinoOta() {
  _buildHostname();
  ArduinoOTA.setHostname(_host);
  #ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(OTA_PASSWORD);
  #endif

  ArduinoOTA
      .onStart([]() {
        _inProgress = true;
        Serial.println(F("[OTA] update started (espota)"));
  #ifdef LFX_OLED_ENABLED
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

  #ifdef LFX_API_SERVER_ENABLED

namespace {

bool _denied = false; // set when a web upload fails auth, to refuse the write/restart

} // namespace

void registerWebRoutes() {
  WebServer &srv = ApiServer::server();

  // The upload UI now lives in the WebUI "About" page (which POSTs here). A
  // direct GET /update just bounces to the app instead of serving a bare page.
  srv.on("/update", HTTP_GET, []() {
    WebServer &s = ApiServer::server();
    s.sendHeader("Location", "/ui");
    s.send(302);
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
          _inProgress = true;
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

  #endif // LFX_API_SERVER_ENABLED

} // namespace OtaUpdater

#endif // LFX_OTA_ENABLED
