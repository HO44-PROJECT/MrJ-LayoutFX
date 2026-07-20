/**
 * @file WifiApi.h
 * @brief Runtime WiFi provisioning — save/load STA credentials on LittleFS,
 *        and the POST /api/wifi endpoint that lets the WebUI set them while
 *        the device is in AP mode (WLED-style captive portal, #132).
 *
 * Deliberately independent of ConfigManager/DeviceFactory: credentials must
 * be loadable before any device config exists (first boot, AP mode, empty
 * config.json), and this module's own LittleFS file is unrelated to the
 * device/board/bus config.
 *
 * @project MrJ-LayoutFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author  MrJ
 * @date    2026-07-20
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_CONFIG_ENABLED

  #include <Arduino.h>

class WifiApi {
public:
  #ifdef LFX_API_SERVER_ENABLED
  /** @brief Register POST /api/wifi and GET /api/wifi/scan on ApiServer. */
  static void init();
  #endif

  /**
   * @brief Load saved STA credentials from LittleFS, if any.
   *
   * @param ssid      Filled with the saved SSID (empty if none saved).
   * @param password  Filled with the saved password (empty if none saved).
   * @return true if credentials were found and loaded.
   */
  static bool load(String &ssid, String &password);

private:
  static constexpr const char *kPath = "/wifi.json"; ///< LittleFS path — separate from the device config.json.

  /** @brief Save SSID/password to LittleFS as JSON. Returns false on write error. */
  static bool _save(const String &ssid, const String &password);

  #ifdef LFX_API_SERVER_ENABLED
  /** @brief POST /api/wifi — body {"ssid":"<s>","password":"<p>"}. Tests STA before saving/rebooting. */
  static void _onPostWifi();

  /** @brief GET /api/wifi/scan — synchronous WiFi.scanNetworks(), strongest signal first. */
  static void _onGetScan();
  #endif
};

#endif // LFX_CONFIG_ENABLED
