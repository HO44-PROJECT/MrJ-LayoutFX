/**
 * @file WifiApi.cpp
 * @brief Implementation of runtime WiFi provisioning (#132).
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/WifiApi.h"

#ifdef LFX_CONFIG_ENABLED

  #include <ArduinoJson.h>
  #include <LittleFS.h>
  #include <algorithm>
  #include <vector>

  #ifdef LFX_API_SERVER_ENABLED
    #include "api/ApiServer.h"
  #endif
  #ifdef LFX_OLED_ENABLED
    #include "oled/OledDisplay.h"
  #endif

using namespace http_status;

bool WifiApi::load(String &ssid, String &password) {
  if (!LittleFS.exists(kPath))
    return false;
  File f = LittleFS.open(kPath, "r");
  if (!f)
    return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err || !doc["ssid"].is<const char *>())
    return false;
  ssid = doc["ssid"].as<const char *>();
  password = doc["password"] | "";
  return ssid.length() > 0;
}

bool WifiApi::_save(const String &ssid, const String &password) {
  JsonDocument doc;
  doc["ssid"] = ssid;
  doc["password"] = password;
  File f = LittleFS.open(kPath, "w");
  if (!f)
    return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

  #ifdef LFX_API_SERVER_ENABLED

/**
 * @brief POST /api/wifi — body {"ssid":"<s>","password":"<p>"}.
 *
 * ESP32 has a single WiFi radio — AP+STA concurrent mode forces the SoftAP
 * onto the STA target's channel the moment it associates (and disrupts it
 * further while scanning to find that channel), so there is no way to test
 * a candidate STA connection while keeping the caller's own AP connection
 * alive to deliver the result (confirmed against ESP-IDF's own docs; WLED,
 * the reference this provisioning flow is modeled on, doesn't attempt it
 * either — it tears its AP down for every credential test, same as here).
 *
 * So: drop the AP, test STA for real (same retry budget as ApiServer::init()'s
 * boot-time attempt), then reboot unconditionally. Credentials are saved
 * *only* on a confirmed connection — a bad password is never persisted, so
 * the reboot's normal boot sequence falls back to the AP on its own (same
 * code path as any other STA failure). The caller's phone loses the AP
 * connection for the ~10s of the test either way; it must reconnect — to
 * the device's AP if it's back (failure), or to its usual network to find
 * the device's new IP via its OLED (success) — there is no way to avoid this
 * round-trip on this hardware.
 */
void WifiApi::_onPostWifi() {
  WebServer &s = ApiServer::server();
  if (!s.hasArg("plain")) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, s.arg("plain")) || !doc["ssid"].is<const char *>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  String ssid = doc["ssid"].as<const char *>();
  String password = doc["password"] | "";
  if (ssid.length() == 0) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"ssid required\"}"));
    return;
  }

  ApiServer::sendJson(kOk, F("{\"ok\":true,\"testing\":true}"));
  #ifdef LFX_OLED_ENABLED
  OledDisplay::showMessage(LFX_PROJECT_NAME, "Test connexion...");
  #endif

  static constexpr uint8_t  kRetries = 20;  // mirrors ApiServer::kWifiRetries
  static constexpr uint16_t kRetryMs = 500; // mirrors ApiServer::kWifiRetryMs

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  for (int i = 0; i < kRetries && WiFi.status() != WL_CONNECTED; i++)
    delay(kRetryMs);

  if (WiFi.status() == WL_CONNECTED)
    _save(ssid, password); // only ever persist confirmed-working credentials

  #ifdef LFX_OLED_ENABLED
  OledDisplay::showMessage(LFX_PROJECT_NAME, "Redemarrage...");
  #endif
  delay(400);
  ESP.restart();
}

/**
 * @brief GET /api/wifi/scan — {"networks":[{"ssid":"<s>","rssi":<n>},...]}.
 *
 * Synchronous WiFi.scanNetworks() (~2-4 s) — acceptable for a one-off
 * provisioning form. Hidden SSIDs (empty string) are skipped, duplicates
 * (multiple APs/channels for the same network) are collapsed to their
 * strongest RSSI, sorted strongest-first.
 */
void WifiApi::_onGetScan() {
  int n = WiFi.scanNetworks();

  struct Net { String ssid; int32_t rssi; };
  std::vector<Net> nets;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0)
      continue;
    int32_t rssi = WiFi.RSSI(i);
    bool found = false;
    for (Net &net : nets) {
      if (net.ssid == ssid) {
        found = true;
        if (rssi > net.rssi)
          net.rssi = rssi;
        break;
      }
    }
    if (!found)
      nets.push_back({ssid, rssi});
  }
  WiFi.scanDelete();

  std::sort(nets.begin(), nets.end(), [](const Net &a, const Net &b) { return a.rssi > b.rssi; });

  JsonDocument doc;
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (const Net &net : nets) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = net.ssid;
    o["rssi"] = net.rssi;
  }

  String body;
  serializeJson(doc, body);
  ApiServer::sendJson(kOk, body);
}

void WifiApi::init() {
  ApiServer::on("/api/wifi", HTTP_POST, _onPostWifi);
  ApiServer::on("/api/wifi/scan", HTTP_GET, _onGetScan);
}

  #endif // LFX_API_SERVER_ENABLED

#endif // LFX_CONFIG_ENABLED
