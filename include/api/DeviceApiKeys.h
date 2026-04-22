/**
 * @file DeviceApiKeys.h
 * @brief JSON field names, sub-object keys, pin labels and LittleFS path constants
 *        used by the DeviceApi implementation files.
 *
 * Centralises every literal that appears in the API JSON responses so that
 * schema renames touch exactly one place.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author  MrJ
 * @date    2026-04-22
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

namespace api_keys {

// ── Top-level status fields ────────────────────────────────────────────────
constexpr char kVersion[] = "version";
constexpr char kBuildDate[] = "build_date";
constexpr char kEnv[] = "env";
constexpr char kUptimeS[] = "uptime_s";
constexpr char kIp[] = "ip";
constexpr char kConfig[] = "config";
constexpr char kDevices[] = "devices";
constexpr char kDevicesMax[] = "devices_max";
constexpr char kCpuMhz[] = "cpu_mhz";
constexpr char kChip[] = "chip";
constexpr char kChipRev[] = "chip_rev";
constexpr char kHeapFree[] = "heap_free";
constexpr char kHeapTotal[] = "heap_total";
constexpr char kHeapMin[] = "heap_min";
constexpr char kTempC[] = "temp_c";
constexpr char kSketchSize[] = "sketch_size";
constexpr char kSketchFree[] = "sketch_free";
constexpr char kFsTotal[] = "fs_total";
constexpr char kFsUsed[] = "fs_used";

// ── "features" sub-object ──────────────────────────────────────────────────
constexpr char kFeatures[] = "features";
constexpr char kFeatWifi[] = "wifi";
constexpr char kFeatApi[] = "api";
constexpr char kFeatWebui[] = "webui";
constexpr char kFeatConfig[] = "config";
constexpr char kFeatOled[] = "oled";
constexpr char kFeatSpi[] = "spi";
constexpr char kFeatLobotServo[] = "lobot_servo";
constexpr char kFeatLx16aServo[] = "lx16a_servo";
constexpr char kFeatDcc[] = "dcc";
constexpr char kFeatAudio[] = "audio";

// ── "sys_pins" sub-object ─────────────────────────────────────────────────
constexpr char kSysPins[] = "sys_pins";
constexpr char kPinTx0[] = "TX0";
constexpr char kPinRx0[] = "RX0";
constexpr char kPinSda[] = "SDA";
constexpr char kPinScl[] = "SCL";
constexpr char kPinDcc[] = "DCC";

// ── WiFi fields ────────────────────────────────────────────────────────────
constexpr char kWifiSsid[] = "wifi_ssid";
constexpr char kWifiRssi[] = "wifi_rssi";
constexpr char kWifiMac[] = "wifi_mac";

// ── "libs" sub-object ─────────────────────────────────────────────────────
constexpr char kLibs[] = "libs";
constexpr char kLibArduinoJson[] = "ArduinoJson";
constexpr char kLibEspIdf[] = "ESP-IDF";
constexpr char kLibArduinoEsp32[] = "Arduino-ESP32";
constexpr char kLibAceRoutine[] = "AceRoutine";
constexpr char kLibNmraDcc[] = "NmraDcc";
constexpr char kLibU8g2[] = "U8g2";

// ── LittleFS paths (shared across DeviceConfigApi and DeviceStatusApi) ─────
constexpr char kPathBoardTypes[] = "/board_types.json";
constexpr char kFileBoardTypes[] = "board_types.json"; ///< Filename only (no leading slash).
constexpr char kPathConfigSource[] = "/config_source.txt";

// ── Device control — POST body fields ─────────────────────────────────────
constexpr char kId[]     = "id";     ///< Device identifier.
constexpr char kState[]  = "state";  ///< Device state value.
constexpr char kOn[]     = "on";     ///< Boolean switch (POST /api/switch).
constexpr char kBoard[]  = "board";  ///< Board index filter (POST /api/all).
constexpr char kType[]   = "type";   ///< Device type filter (POST /api/group).
constexpr char kAction[] = "action"; ///< Servo action string (POST /api/servo).
constexpr char kSpeed[]  = "speed";  ///< Servo speed value (POST /api/servo).

// ── Hardware test — POST body fields ──────────────────────────────────────
constexpr char kPin[]     = "pin";     ///< GPIO pin number (POST /api/test/gpio).
constexpr char kCard[]    = "card";    ///< SPI card index (POST /api/test/spi).
constexpr char kChannel[] = "channel"; ///< SPI channel index (POST /api/test/spi).

// ── I2C scan — response fields ─────────────────────────────────────────────
constexpr char kSda[]   = "sda";   ///< SDA pin used for scan (lowercase, not the sys_pin label).
constexpr char kScl[]   = "scl";   ///< SCL pin used for scan (lowercase).
constexpr char kFound[] = "found"; ///< Array of found I2C addresses.
constexpr char kCount[] = "count"; ///< Number of found addresses.

// ── Config file management — JSON body and URL param fields ───────────────
constexpr char kName[] = "name"; ///< Config display name (JSON field and ?name= query param).
constexpr char kFile[] = "file"; ///< Config filename (DELETE /api/configs body).
constexpr char kFrom[] = "from"; ///< Source filename for copy/rename.
constexpr char kTo[]   = "to";   ///< Destination filename for copy/rename.

// ── WebServer arg keys ────────────────────────────────────────────────────
constexpr char kArgPlain[] = "plain"; ///< Raw request body arg key (WebServer::arg("plain")).

} // namespace api_keys
