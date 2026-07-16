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
constexpr char kFeatI2c[]  = "i2c";
constexpr char kFeatSpi[]  = "spi";
constexpr char kFeatLobotServo[] = "lobot_servo";
constexpr char kFeatLx16aServo[] = "lx16a_servo";
constexpr char kFeatDcc[] = "dcc";
constexpr char kFeatAudio[] = "audio";
constexpr char kFeatOta[] = "ota";
// Logging sinks
constexpr char kFeatLogSerial[]   = "log_serial";
constexpr char kFeatDebugSerial[] = "debug_serial";
constexpr char kFeatLogOled[]     = "log_oled";
constexpr char kFeatDebugOled[]   = "debug_oled";
// OLED options
constexpr char kFeatOledStatus[]  = "oled_status";
constexpr char kFeatOledSplash[]  = "oled_splash";
constexpr char kFeatOledMetrics[] = "oled_metrics";
constexpr char kFeatOledEvents[]  = "oled_events";
// Network / bus / behaviour options
constexpr char kFeatWifiForceAp[] = "wifi_force_ap";
constexpr char kFeatDccAudit[]    = "dcc_audit";
constexpr char kFeatI2cScan[]     = "i2c_scan";
constexpr char kFeatJtag[]        = "jtag";
constexpr char kFeatDemo[]        = "demo";

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
constexpr char kPathBoardTypes[]   = "/board_types.json";
constexpr char kFileBoardTypes[]   = "board_types.json";   ///< Filename only (no leading slash).
constexpr char kPathDeviceTypes[]  = "/device_types.json";
constexpr char kFileDeviceTypes[]  = "device_types.json";
constexpr char kPathBusTypes[]     = "/bus_types.json";
constexpr char kFileBusTypes[]     = "bus_types.json";
constexpr char kPathI2cKnown[]     = "/i2c_known.json";
constexpr char kFileI2cKnown[]     = "i2c_known.json";
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
constexpr char kLow[]     = "low";     ///< Other GPIOs to hold LOW for a charlieplex wiring test.

// ── I2C scan — response fields ─────────────────────────────────────────────
constexpr char kSda[]   = "sda";   ///< SDA pin used for scan (lowercase, not the sys_pin label).
constexpr char kScl[]   = "scl";   ///< SCL pin used for scan (lowercase).
constexpr char kFound[] = "found"; ///< Array of found I2C addresses.
constexpr char kCount[] = "count"; ///< Number of found addresses.

// ── DCC status — response fields (GET /api/dcc-status) ────────────────────
constexpr char kDccEnabled[] = "enabled"; ///< Whether LFX_DCC_ENABLED is compiled in.
constexpr char kDccUptimeMs[] = "uptime_ms"; ///< millis() at response time, for client-side age calc.
constexpr char kDccMessages[] = "messages"; ///< Per-category counters object.
constexpr char kDccCount[] = "count"; ///< Packets seen since boot, for one category.
constexpr char kDccLastMs[] = "last_ms"; ///< millis() of the last packet of this category (0 = never).
constexpr char kDccKindRaw[] = "raw"; ///< Any raw packet on the bus (proves the bus is alive).
constexpr char kDccKindSpeed[] = "speed"; ///< Speed/direction packets.
constexpr char kDccKindFunc[] = "func"; ///< Function group packets (F0-F12).
constexpr char kDccKindAccessory[] = "accessory"; ///< Basic accessory (turnout) packets.
constexpr char kDccKindSignal[] = "signal"; ///< Extended accessory (signal aspect) packets.
constexpr char kDccLog[] = "log"; ///< Array of decoded events, oldest first (see DccDrivable::DccLogEntry).
constexpr char kDccLogKind[] = "kind"; ///< Message category of a log entry (same strings as kDccKind*).
constexpr char kDccLogAddress[] = "address"; ///< DCC address the log entry's packet targeted.
constexpr char kDccLogValue[] = "value"; ///< Decoded value (mapped speed, state, or aspect).
constexpr char kDccLogDevice[] = "device"; ///< Name of the device that reacted, or "" if no device matched.
constexpr char kDccLogRepeat[] = "repeat"; ///< Consecutive identical packets collapsed into this entry (1 = no repeat).

// ── Config file management — JSON body and URL param fields ───────────────
constexpr char kName[] = "name"; ///< Config display name (JSON field and ?name= query param).
constexpr char kFile[] = "file"; ///< Config filename (DELETE /api/configs body).
constexpr char kFrom[] = "from"; ///< Source filename for copy/rename.
constexpr char kTo[]   = "to";   ///< Destination filename for copy/rename.

// ── WebServer arg keys ────────────────────────────────────────────────────
constexpr char kArgPlain[] = "plain"; ///< Raw request body arg key (WebServer::arg("plain")).

} // namespace api_keys
