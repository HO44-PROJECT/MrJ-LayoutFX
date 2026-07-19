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
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author  MrJ
 * @date    2026-04-22
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_API_SERVER_ENABLED

  #include "utils/utils.h"
  #include <Arduino.h>
  #include <DNSServer.h>
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
   * @brief Connect WiFi in STA mode; fall back to AP mode if STA fails.
   *        Starts the HTTP server and launches the Core-0 system task.
   *
   * @param ssid        STA WiFi SSID.
   * @param password    STA WiFi password.
   * @param apSsid      AP fallback SSID (default: WIFI_AP_SSID).
   * @param apPassword  AP fallback password (default: WIFI_AP_PASSWORD, min 8 chars or "").
   * @param port        HTTP port (default: LFX_API_HTTP_PORT).
   * @param forceAp     Skip STA and start the SoftAP directly (recovery/safe mode).
   */
  static void init(const char *ssid, const char *password,
                   const char *apSsid, const char *apPassword,
                   uint16_t port = LFX_API_HTTP_PORT,
                   bool forceAp = false);

  /** @brief Return true if the server is running in AP (access-point) mode. */
  static bool isAP() { return _isAP; }

private:
  static constexpr uint8_t  kWifiRetries      = 20;   ///< Max STA connection attempts before AP fallback.
  static constexpr uint16_t kWifiRetryMs      = 500;  ///< Delay between each attempt (ms).

  #ifdef LFX_OTA_ENABLED
  static constexpr uint32_t kTaskStackBytes   = 8192; ///< Larger: web /update (Update.write) runs in this task.
  #else
  static constexpr uint32_t kTaskStackBytes   = 4096; ///< Stack size for the Core-0 system task.
  #endif
  static constexpr uint8_t  kTaskPriority     = 1;    ///< FreeRTOS priority of the system task.
  static constexpr uint8_t  kTaskCore         = 0;    ///< CPU core for the system task (same as WiFi stack).
  static constexpr uint8_t  kTaskYieldTicks   = 1;    ///< vTaskDelay ticks between handleClient() calls.

  static WebServer *_server;
  static bool       _isAP;
  static DNSServer *_dns;

  /**
   * @brief Lazily create the WebServer with the given port (no-op if already created).
   *
   * @param port HTTP port number (default 80).
   * @return Reference to the singleton WebServer instance.
   */
  static WebServer &_get(uint16_t port = LFX_API_HTTP_PORT);

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
