/**
 * @file ApiServer.cpp
 * @brief WiFi + HTTP server implementation — singleton WebServer lifecycle,
 *        CORS headers, route registration, and Core-0 system task.
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/ApiServer.h"

#ifdef LFX_API_SERVER_ENABLED
  #include <DNSServer.h>

using namespace http_status;

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

WebServer *ApiServer::_server = nullptr;
bool ApiServer::_isAP = false;
DNSServer *ApiServer::_dns = nullptr;

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

/**
 * @brief Lazily create the WebServer singleton with the given port.
 *        No-op if the server is already created (first call wins).
 * @param port HTTP port number (default LFX_API_HTTP_PORT).
 * @return Reference to the singleton WebServer instance.
 */
WebServer &ApiServer::_get(uint16_t port) {
  if (!_server)
    _server = new WebServer(port);
  return *_server;
}

/**
 * @brief Access the underlying WebServer (for use inside handlers).
 * @return Reference to the singleton WebServer instance.
 */
WebServer &ApiServer::server() {
  return _get();
}

/**
 * @brief Send a JSON response with CORS headers guaranteed.
 * @param code HTTP status code.
 * @param body Response body as an Arduino String.
 */
void ApiServer::sendJson(int code, const String &body) {
  _server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  _server->send(code, "application/json", body);
}

/**
 * @brief Send a JSON response with CORS headers guaranteed (PROGMEM overload).
 * @param code HTTP status code.
 * @param body Response body as a flash-stored string literal.
 */
void ApiServer::sendJson(int code, const __FlashStringHelper *body) {
  _server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  _server->send(code, "application/json", body);
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

/**
 * @brief Preflight response for CORS — registered automatically on every route.
 *        Responds 204 with Access-Control-Allow-* headers.
 */
void ApiServer::_onOptions() {
  _server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  _server->sendHeader(F("Access-Control-Allow-Methods"), F("GET,POST,DELETE,OPTIONS"));
  _server->sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type,Accept,X-Config-Name"));
  _server->send(kNoContent);
}

/**
 * @brief Register an HTTP route and its CORS preflight handler.
 *        Must be called before init().
 * @param path    URL path (e.g. "/api/status").
 * @param method  HTTP method (HTTP_GET, HTTP_POST, …).
 * @param handler Callback to invoke when the route is matched.
 */
void ApiServer::on(const char *path, HTTPMethod method,
                   WebServer::THandlerFunction handler) {
  _get().on(path, method, handler);
  _get().on(path, HTTP_OPTIONS, _onOptions);
}

/**
 * @brief Connect to WiFi in STA mode; fall back to AP mode if STA fails.
 *        Starts the HTTP server and launches the Core-0 system task.
 *
 * Decision flow:
 *   1. startApDirect (forceAp, no ssid, or LFX_WIFI_FORCE_AP) → AP straight away,
 *      skipping the STA retry loop for an attempt that can't succeed.
 *   2. Otherwise, try STA for up to kWifiRetries * kWifiRetryMs; on success the
 *      device joins the caller's network at WiFi.localIP().
 *   3. If STA fails (or step 1 applied), _startAP() opens a fallback access
 *      point at apSsid/apPassword and turns on the captive portal: a DNSServer
 *      resolving every hostname to the AP's own IP, plus OS probe routes
 *      (/generate_204, /hotspot-detect.html) and the onNotFound handler below,
 *      all redirecting to /ui — so joining the AP pops the WiFi-setup page
 *      automatically on phones/laptops, no URL typing needed (#132).
 *
 * _isAP tracks which mode is live so onNotFound() below only does the
 * captive-portal redirect in AP mode (STA 404s stay plain JSON errors).
 *
 * @param ssid       STA network SSID.
 * @param password   STA network password.
 * @param apSsid     AP fallback SSID.
 * @param apPassword AP fallback password (min 8 chars, or "" for open network).
 * @param port       HTTP port (default LFX_API_HTTP_PORT).
 */
void ApiServer::init(const char *ssid, const char *password,
                     const char *apSsid, const char *apPassword,
                     uint16_t port, bool forceAp) {
  Serial.setDebugOutput(false); // prevent ESP-IDF binary logs leaking on UART0

  // Ensure server exists with the requested port (first call wins).
  _get(port);

  // Log AP client connect/disconnect events.
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
      Serial.printf("[WiFi] client connected   — MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                    info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                    info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                    info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
    } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
      Serial.println(F("[WiFi] client disconnected"));
    }
  });

  // Opens the fallback AP + captive portal. Called either directly (forced AP)
  // or after a failed STA attempt below — never both, so _isAP unambiguously
  // reflects which mode is actually live once init() returns.
  auto _startAP = [&]() {
    WiFi.mode(WIFI_AP);
    const char *apPwd = (apPassword && strlen(apPassword) >= 8) ? apPassword : nullptr;
    if (WiFi.softAP(apSsid, apPwd)) {
      _isAP = true;
      Serial.print(F("[WiFi] AP SSID: "));
      Serial.println(apSsid);
      Serial.print(F("[WiFi] AP IP:   "));
      Serial.println(WiFi.softAPIP());
      // Captive portal DNS — redirect every hostname to the AP IP.
      _dns = new DNSServer();
      _dns->start(53, "*", WiFi.softAPIP());
      // HTTP probe routes: Android fallback (/generate_204) and macOS (/hotspot-detect.html).
      // These are hit when the OS still tries HTTP after HTTPS fails. Built from
      // WiFi.softAPIP() rather than hardcoded — 192.168.4.1 is only the default
      // softAP IP, not guaranteed (custom AP subnet, future config option, ...).
      auto _cpRedirect = []() {
        _server->sendHeader(F("Location"), "http://" + WiFi.softAPIP().toString() + "/ui");
        _server->send(302);
      };
      _server->on("/generate_204", HTTP_GET, _cpRedirect);
      _server->on("/hotspot-detect.html", HTTP_GET, _cpRedirect);
    } else {
      Serial.println(F("[WiFi] AP failed"));
    }
  };

  // Force AP when the caller asks (safe mode), when compiled-in, or when
  // there's no SSID to even try (e.g. web_installer with no runtime creds
  // saved yet, #132) — skips the ~10s STA retry loop for a doomed attempt.
  bool startApDirect = forceAp || !ssid || !ssid[0];
  #ifdef LFX_WIFI_FORCE_AP
  startApDirect = true;
  #endif

  if (startApDirect) {
    // --- AP forced ---
    Serial.println(F("[WiFi] AP mode forced"));
    _startAP();
  } else {
    // --- STA ---
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    LOG_PRINT(F("[WiFi] connecting to "));
    LOG_PRINTLN(ssid);
    for (int i = 0; i < kWifiRetries && WiFi.status() != WL_CONNECTED; i++) {
      delay(kWifiRetryMs);
      LOG_PRINT('.');
    }
    LOG_PRINTLN();

    if (WiFi.status() == WL_CONNECTED) {
      _isAP = false;
      Serial.print(F("[WiFi] STA IP: "));
      Serial.println(WiFi.localIP());
    } else {
      // --- AP fallback ---
      Serial.println(F("[WiFi] STA failed — starting AP"));
      _startAP();
    }
  }

  // --- Not-found handler: CORS preflight + captive-portal redirect in AP mode ---
  _server->onNotFound([]() {
    if (_server->method() == HTTP_OPTIONS) {
      _server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      _server->sendHeader(F("Access-Control-Allow-Methods"), F("GET,POST,DELETE,OPTIONS"));
      _server->sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type,Accept,X-Config-Name"));
      _server->send(kNoContent);
    } else if (_isAP) {
      // Redirect captive-portal probes (iOS, Android, Windows) to the web UI.
      _server->sendHeader(F("Location"), "http://" + WiFi.softAPIP().toString() + "/ui");
      _server->send(302);
    } else {
      _server->send(kNotFound, "application/json", F("{\"error\":\"not found\"}"));
    }
  });

  // --- Start HTTP server ---
  _server->begin();
  LOG_PRINT(F("[HTTP] port "));
  LOG_PRINTLN(port);

  // --- System task on Core 0 (same core as WiFi stack) ---
  // All non-coroutine work: HTTP + DCC. Core 1 (loop) = CoroutineScheduler only.
  xTaskCreatePinnedToCore(
      [](void *) {
        for (;;) {
          if (_dns)
            _dns->processNextRequest();
          // Serialise request handlers (which read/mutate the device list) against
          // the Core-1 hot-reload that deletes every Device (backlog #27).
          {
            LFX_DEVICE_LOCK();
            _server->handleClient();
          }
          vTaskDelay(kTaskYieldTicks);
        }
      },
      "system", kTaskStackBytes, nullptr, kTaskPriority, nullptr, kTaskCore);
}

#endif // LFX_API_SERVER_ENABLED
