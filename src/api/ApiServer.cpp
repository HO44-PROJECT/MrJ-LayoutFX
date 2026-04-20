/**
 * @file ApiServer.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/ApiServer.h"
#include "utils/utils.h"

#ifdef MRJFX_WEBUI_ENABLED

  #include <WiFi.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

WebServer *ApiServer::_server = nullptr;

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

WebServer &ApiServer::_get(uint16_t port) {
  if (!_server)
    _server = new WebServer(port);
  return *_server;
}

WebServer &ApiServer::server() {
  return _get();
}

void ApiServer::sendJson(int code, const String &body) {
  _server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  _server->send(code, "application/json", body);
}

void ApiServer::sendJson(int code, const __FlashStringHelper *body) {
  _server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  _server->send(code, "application/json", body);
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ApiServer::_onOptions() {
  _server->sendHeader(F("Access-Control-Allow-Origin"),  F("*"));
  _server->sendHeader(F("Access-Control-Allow-Methods"), F("GET,POST,DELETE,OPTIONS"));
  _server->sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type,Accept,X-Config-Name"));
  _server->send(204);
}

void ApiServer::on(const char *path, HTTPMethod method,
                   WebServer::THandlerFunction handler) {
  _get().on(path, method, handler);
  _get().on(path, HTTP_OPTIONS, _onOptions);
}

void ApiServer::init(const char *ssid, const char *password, uint16_t port) {
  Serial.setDebugOutput(false); // prevent ESP-IDF binary logs leaking on UART0

  // Ensure server exists with the requested port (first call wins).
  _get(port);

  // --- WiFi ---
  WiFi.begin(ssid, password);
  LOG_PRINT(F("[WiFi] connecting"));
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
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
      _server->sendHeader(F("Access-Control-Allow-Origin"),  F("*"));
      _server->sendHeader(F("Access-Control-Allow-Methods"), F("GET,POST,DELETE,OPTIONS"));
      _server->sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type,Accept,X-Config-Name"));
      _server->send(204);
    } else {
      _server->send(404, "application/json", F("{\"error\":\"not found\"}"));
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
          vTaskDelay(1); // yield 1 tick (1 ms) to WiFi stack
        }
      },
      "system", 4096, nullptr, 1, nullptr, 0 // Core 0
  );
}

#endif // MRJFX_WEBUI_ENABLED
