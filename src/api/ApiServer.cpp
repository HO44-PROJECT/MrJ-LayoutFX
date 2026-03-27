/**
 * @file ApiServer.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#ifdef ESP32

#include "api/ApiServer.h"
#include "dcc/DccDrivable.h"
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

WebServer* ApiServer::_server = nullptr;

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

WebServer& ApiServer::_get(uint16_t port) {
    if (!_server) _server = new WebServer(port);
    return *_server;
}

WebServer& ApiServer::server() {
    return _get();
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ApiServer::on(const char* path, HTTPMethod method,
                   WebServer::THandlerFunction handler) {
    _get().on(path, method, handler);
}

void ApiServer::init(const char* ssid, const char* password, uint16_t port) {
    Serial.setDebugOutput(false);

    // Ensure server exists with the requested port (first call wins).
    _get(port);

    // --- WiFi ---
    WiFi.begin(ssid, password);
    Serial.print(F("[WiFi] connecting"));
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print(F("[WiFi] IP: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("[WiFi] not connected"));
    }

    // --- Start HTTP server ---
    _server->begin();
    Serial.print(F("[HTTP] port "));
    Serial.println(port);

    // --- System task on Core 0 (same core as WiFi stack) ---
    // All non-coroutine work: HTTP + DCC. Core 1 (loop) = CoroutineScheduler only.
    xTaskCreatePinnedToCore(
        [](void*) {
            for (;;) {
                _server->handleClient();
                DccDrivable::loop();
                vTaskDelay(1);   // yield 1 tick (1 ms) to WiFi stack
            }
        },
        "system", 4096, nullptr, 1, nullptr, 0   // Core 0
    );

}

#endif  // ESP32
