/**
 * @file ApiServer.cpp
 * @brief WiFi + HTTP server implementation — singleton WebServer lifecycle,
 *        CORS headers, route registration, and Core-0 system task.
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/ApiServer.h"

#ifdef MRJFX_WEBUI_ENABLED

using namespace http_status;

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

WebServer *ApiServer::_server = nullptr;

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

/**
 * @brief Lazily create the WebServer singleton with the given port.
 *        No-op if the server is already created (first call wins).
 * @param port HTTP port number (default MRJFX_API_HTTP_PORT).
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
 * @brief Connect to WiFi, start the HTTP server, and launch the Core-0 system task.
 *        Also calls CoroutineScheduler::setup() so coroutines start on Core 1.
 * @param ssid     WiFi network SSID.
 * @param password WiFi network password.
 * @param port     HTTP port (default MRJFX_API_HTTP_PORT).
 */
void ApiServer::init(const char *ssid, const char *password, uint16_t port) {
  Serial.setDebugOutput(false); // prevent ESP-IDF binary logs leaking on UART0

  // Ensure server exists with the requested port (first call wins).
  _get(port);

  // --- WiFi ---
  WiFi.begin(ssid, password);
  LOG_PRINT(F("[WiFi] connecting"));
  for (int i = 0; i < kWifiRetries && WiFi.status() != WL_CONNECTED; i++) {
    delay(kWifiRetryMs);
    LOG_PRINT('.');
  }
  LOG_PRINTLN();
  if (WiFi.status() == WL_CONNECTED) {
    // Always print IP — critical info needed even when LOG_SERIAL is not defined.
    Serial.print(F("[WiFi] IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("[WiFi] not connected"));
  }

  // --- Preflight handler for CORS (OPTIONS) ---
  _server->onNotFound([]() {
    if (_server->method() == HTTP_OPTIONS) {
      _server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      _server->sendHeader(F("Access-Control-Allow-Methods"), F("GET,POST,DELETE,OPTIONS"));
      _server->sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type,Accept,X-Config-Name"));
      _server->send(kNoContent);
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
          _server->handleClient();
          vTaskDelay(kTaskYieldTicks);
        }
      },
      "system", kTaskStackBytes, nullptr, kTaskPriority, nullptr, kTaskCore);
}

#endif // MRJFX_WEBUI_ENABLED
