/**
 * @file ApiServer.h
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
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author  MrJ
 * @date    2026-04-22
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include <MrJRailwayFX_define.h>

#ifdef MRJFX_API_SERVER_ENABLED

  #include "utils/utils.h"
  #include <Arduino.h>
  #include <WebServer.h>
  #include <WiFi.h>

class ApiServer {
public:
  /**
   * @brief Register an HTTP route.
   *        Must be called before init().
   */
  static void on(const char *path, HTTPMethod method,
                 WebServer::THandlerFunction handler);

  /**
   * @brief Access the underlying WebServer (for use inside handlers).
   */
  static WebServer &server();

  /**
   * @brief Send a JSON response with CORS headers guaranteed.
   */
  static void sendJson(int code, const String &body);
  static void sendJson(int code, const __FlashStringHelper *body);

  /**
   * @brief Connect WiFi, start HTTP server, launch Core-0 system task,
   *        then call CoroutineScheduler::setup().
   *
   * @param ssid      WiFi SSID.
   * @param password  WiFi password.
   * @param port      HTTP port (default 80).
   */
  static void init(const char *ssid, const char *password, uint16_t port = MRJFX_API_HTTP_PORT);

private:
  static constexpr uint8_t  kWifiRetries      = 40;   ///< Max connection attempts before giving up.
  static constexpr uint16_t kWifiRetryMs      = 500;  ///< Delay between each attempt (ms). Total = retries × delay.

  static constexpr uint32_t kTaskStackBytes   = 4096; ///< Stack size for the Core-0 system task.
  static constexpr uint8_t  kTaskPriority     = 1;    ///< FreeRTOS priority of the system task.
  static constexpr uint8_t  kTaskCore         = 0;    ///< CPU core for the system task (same as WiFi stack).
  static constexpr uint8_t  kTaskYieldTicks   = 1;    ///< vTaskDelay ticks between handleClient() calls.

  static WebServer *_server;

  /**
   * @brief Lazily create the WebServer with the given port (no-op if already created).
   *
   * @param port HTTP port number (default 80).
   * @return Reference to the singleton WebServer instance.
   */
  static WebServer &_get(uint16_t port = MRJFX_API_HTTP_PORT);

  /**
   * @brief Preflight response for CORS — registered automatically on every route.
   *        Responds 204 with Access-Control-Allow-* headers.
   */
  static void _onOptions();
};

/** @brief Standard HTTP status codes used across all API endpoints. */
namespace http_status {
  static constexpr int kOk                  = 200; ///< Request succeeded.
  static constexpr int kNoContent           = 204; ///< Success with no body (CORS preflight).
  static constexpr int kBadRequest          = 400; ///< Malformed request or missing body.
  static constexpr int kForbidden           = 403; ///< Valid request but refused (e.g. reserved pin).
  static constexpr int kNotFound            = 404; ///< Resource does not exist.
  static constexpr int kInternalError       = 500; ///< Server-side failure (write error, etc.).
  static constexpr int kNotImplemented      = 501; ///< Feature not compiled in.
  static constexpr int kServiceUnavailable  = 503; ///< Hardware not ready.
} // namespace http_status

#endif // ESP32
