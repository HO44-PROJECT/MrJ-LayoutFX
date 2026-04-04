/**
 * @file ApiServer.h
 *
 * @brief Generic WiFi + HTTP server infrastructure (ESP32 only).
 *
 * Manages WiFi connection, WebServer lifecycle, and the Core-0 system task.
 * Modules register their own endpoints via ApiServer::on() before calling init().
 * Core 1 (loop) is left exclusively for CoroutineScheduler.
 *
 * Typical usage
 * -------------
 *   // In each module's init — register endpoints first:
 *   ApiServer::on("/config", HTTP_GET, myHandler);
 *
 *   // Then start everything:
 *   ApiServer::init(WIFI_SSID, WIFI_PASSWORD);
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_API_SERVER_ENABLED

#include <Arduino.h>
#include <WebServer.h>

class ApiServer {
public:
    /**
     * @brief Register an HTTP route.
     *        Must be called before init().
     */
    static void on(const char* path, HTTPMethod method,
                   WebServer::THandlerFunction handler);

    /**
     * @brief Access the underlying WebServer (for use inside handlers).
     */
    static WebServer& server();

    /**
     * @brief Connect WiFi, start HTTP server, launch Core-0 system task,
     *        then call CoroutineScheduler::setup().
     *
     * @param ssid      WiFi SSID.
     * @param password  WiFi password.
     * @param port      HTTP port (default 80).
     */
    static void init(const char* ssid, const char* password, uint16_t port = 80);

private:
    static WebServer* _server;

    /** Lazily create the WebServer with the given port (no-op if already created). */
    static WebServer& _get(uint16_t port = 80);
};

#endif  // ESP32
