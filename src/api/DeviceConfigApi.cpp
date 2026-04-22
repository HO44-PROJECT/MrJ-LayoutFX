/**
 * @file DeviceConfigApi.cpp
 * @brief DeviceApi — config file management endpoints (/api/config*, /api/configs*).
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/DeviceApi.h"

#ifdef MRJFX_API_SERVER_ENABLED

using namespace api_keys;
using namespace http_status;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/**
 * @brief Validate and normalise a config filename in-place.
 *        Appends ".json" if missing, rejects names longer than kFsNameMax,
 *        and rejects characters other than alphanumeric, '_', '-', '.'.
 * @param name Filename to validate (modified in-place).
 * @return true if the name is safe to use as a LittleFS path.
 */
bool DeviceApi::_sanitizeCfgName(String &name) {
  if (!name.endsWith(".json"))
    name += ".json";
  if (name.length() > kFsNameMax)
    return false;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (!isAlphaNumeric(c) && c != '_' && c != '-' && c != '.')
      return false;
  }
  return true;
}

/**
 * @brief Write a byte buffer to an open File in kFsChunkSize-byte chunks.
 * @param f   Open writable File handle.
 * @param buf Data to write.
 * @param len Number of bytes to write.
 * @return true if all bytes were written successfully.
 */
bool DeviceApi::_writeAllBytes(File &f, const uint8_t *buf, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    size_t toWrite = min(len - offset, ConfigManager::kFsChunkSize);
    size_t w = f.write(buf + offset, toWrite);
    if (w == 0) {
      LOG_PRINTF("[FS] write stalled at offset=%u\n", offset);
      return false;
    }
    offset += w;
  }
  return true;
}

/**
 * @brief Streaming file copy within LittleFS — reads src in kFsChunkSize chunks.
 *        If dst already exists it is removed before writing.
 * @param src Source LittleFS path (e.g. "/config_backup.json").
 * @param dst Destination LittleFS path.
 * @return true on success, false on any open or write error.
 */
bool DeviceApi::_copyFile(const char *src, const char *dst) {
  File in = LittleFS.open(src, "r");
  if (!in) {
    LOG_PRINTF("[FS] open(%s,r) failed\n", src);
    return false;
  }
  if (LittleFS.exists(dst))
    LittleFS.remove(dst);
  File out = LittleFS.open(dst, "w");
  if (!out) {
    in.close();
    LOG_PRINTF("[FS] open(%s,w) failed errno=%d\n", dst, errno);
    return false;
  }
  uint8_t buf[ConfigManager::kFsChunkSize];
  size_t total = 0;
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0)
      break;
    size_t w = out.write(buf, n);
    total += w;
    if (w != n) {
      LOG_PRINTF("[FS] write short: %u/%u at offset=%u\n", w, n, total);
      in.close();
      out.close();
      return false;
    }
  }
  in.close();
  out.close();
  LOG_PRINTF("[FS] copyFile %s -> %s (%u bytes)\n", src, dst, total);
  return true;
}

/**
 * @brief Rename a LittleFS file by copying then deleting the source.
 *        LittleFS has no native rename; copy-then-delete is the safe alternative.
 * @param from Source path.
 * @param to   Destination path.
 * @return true on success.
 */
bool DeviceApi::_safeRename(const char *from, const char *to) {
  if (!_copyFile(from, to))
    return false;
  LittleFS.remove(from);
  return true;
}

// ---------------------------------------------------------------------------
// Config — simple read / write / delete
// ---------------------------------------------------------------------------

/** @brief Download the active config.json as an attachment. Returns 404 if absent. */
void DeviceApi::_onGetConfig() {
  LOG_PRINTLN(F("API: GET /api/config"));
  if (!ConfigManager::configExists()) {
    ApiServer::sendJson(kNotFound, F("{\"error\":\"config not found\"}"));
    return;
  }
  ApiServer::server().sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  ApiServer::sendJson(kOk, ConfigManager::readConfig());
}

/** @brief Overwrite config.json with the raw JSON body. No reboot triggered. */
void DeviceApi::_onPostConfig() {
  LOG_PRINTLN(F("API: POST /api/config"));
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  if (!ConfigManager::writeConfig(ApiServer::server().arg(kArgPlain))) {
    ApiServer::sendJson(kInternalError, F("{\"error\":\"write failed\"}"));
    return;
  }
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

/** @brief Delete config.json then trigger an immediate ESP32 restart. */
void DeviceApi::_onDeleteConfig() {
  LOG_PRINTLN(F("API: DELETE /api/config"));
  ConfigManager::deleteConfig();
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
  delay(200);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// Config — list all configs
// ---------------------------------------------------------------------------

/**
 * @brief Return a JSON object listing all .json config files with an "active" marker.
 *        Format: {"active":"<name>","files":[{"file":"<f>","name":"<display>"},…]}.
 *        The active file is determined from /config_source.txt, falling back to config.json.
 */
void DeviceApi::_onGetConfigs() {
  LOG_PRINTLN(F("API: GET /api/configs"));
  String activeName;
  File src = LittleFS.exists(kPathConfigSource) ? LittleFS.open(kPathConfigSource, "r") : File();
  if (src) {
    activeName = src.readString();
    src.close();
    if (activeName.startsWith("/"))
      activeName = activeName.substring(1);
  }
  if (activeName.isEmpty()) {
    activeName = String(ConfigManager::configPath());
    if (activeName.startsWith("/"))
      activeName = activeName.substring(1);
  }
  String fileArr = "[";
  bool first = true;
  File root = LittleFS.open("/");
  File entry = root.openNextFile();
  while (entry) {
    String fname = entry.name();
    if (fname.startsWith("/"))
      fname = fname.substring(1);
    if (fname.endsWith(".json") && fname != kFileBoardTypes) {
      String content = ConfigManager::readFile(("/" + fname).c_str());
      String cfgName = "";
      if (!content.isEmpty()) {
        JsonDocument doc;
        if (!deserializeJson(doc, content) && doc[kName].is<const char *>())
          cfgName = doc[kName].as<String>();
      }
      cfgName.replace("\\", "\\\\");
      cfgName.replace("\"", "\\\"");
      if (!first)
        fileArr += ",";
      fileArr += "{\"file\":\"" + fname + "\",\"name\":\"" + cfgName + "\"}";
      first = false;
    }
    entry = root.openNextFile();
  }
  fileArr += "]";
  String json = F("{\"active\":\"");
  json += activeName;
  json += F("\",\"files\":");
  json += fileArr;
  json += "}";
  ApiServer::sendJson(kOk, json);
}

// ---------------------------------------------------------------------------
// Config — named file CRUD
// ---------------------------------------------------------------------------

/** @brief Download a named config file as attachment. Query param: ?name=<filename>. */
void DeviceApi::_onGetNamedConfig() {
  LOG_PRINTLN(F("API: GET /api/config/file"));
  String name = ApiServer::server().arg(kName);
  if (!_sanitizeCfgName(name)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  String path = "/" + name;
  if (!LittleFS.exists(path.c_str())) {
    ApiServer::sendJson(kNotFound, F("{\"error\":\"file not found\"}"));
    return;
  }
  String cd = "attachment; filename=\"";
  cd += name;
  cd += "\"";
  ApiServer::server().sendHeader("Content-Disposition", cd);
  ApiServer::server().sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  File f = LittleFS.open(path.c_str(), "r");
  ApiServer::server().streamFile(f, "application/json");
  f.close();
}

/**
 * @brief Upload and save a new named config file to LittleFS.
 *        Filename from X-Config-Name header (or ?name= param). Body: raw JSON.
 *        Rejects attempts to overwrite the active config.json directly.
 */
void DeviceApi::_onPostNamedConfig() {
  LOG_PRINTLN(F("API: POST /api/configs"));
  String name = ApiServer::server().header("X-Config-Name");
  if (name.isEmpty())
    name = ApiServer::server().arg(kName);
  bool hasBody = ApiServer::server().hasArg(kArgPlain) && ApiServer::server().arg(kArgPlain).length() > 0;
  LOG_PRINTF("[upload] name='%s' hasBody=%d argCount=%d\n",
             name.c_str(), (int)hasBody, ApiServer::server().args());
  if (name.isEmpty() || !hasBody) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"filename and body required\"}"));
    return;
  }
  if (!_sanitizeCfgName(name)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  if (name == "config.json") {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"use POST /api/config to overwrite active config\"}"));
    return;
  }
  const String &content = ApiServer::server().arg(kArgPlain);
  String path = "/" + name;
  LOG_PRINTF("[upload] name=%s content_len=%u\n", name.c_str(), content.length());
  if (LittleFS.exists(path.c_str()))
    LittleFS.remove(path.c_str());
  errno = 0;
  File f = LittleFS.open(path.c_str(), "w");
  LOG_PRINTF("[upload] open(%s,w) ok=%d errno=%d\n", path.c_str(), (int)(bool)f, errno);
  if (!f) {
    ApiServer::sendJson(kInternalError, F("{\"error\":\"write failed\"}"));
    return;
  }
  bool ok = _writeAllBytes(f, (const uint8_t *)content.c_str(), content.length());
  f.close();
  LOG_PRINTF("[FS] saved %s ok=%d\n", path.c_str(), (int)ok);
  if (!ok) {
    ApiServer::sendJson(kInternalError, F("{\"error\":\"write incomplete\"}"));
    return;
  }
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

/**
 * @brief Delete a named config file from LittleFS.
 *        Body: {"file":"<filename>"}. Rejects deletion of config.json.
 */
void DeviceApi::_onDeleteNamedConfig() {
  LOG_PRINTLN(F("API: DELETE /api/configs"));
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) || !doc[kFile].is<const char *>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"expected {file}\"}"));
    return;
  }
  String name = doc[kFile].as<const char *>();
  if (!_sanitizeCfgName(name) || name == "config.json") {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  String path = "/" + name;
  if (!LittleFS.exists(path.c_str())) {
    ApiServer::sendJson(kNotFound, F("{\"error\":\"file not found\"}"));
    return;
  }
  LittleFS.remove(path.c_str());
  LOG_PRINTF("[FS] deleted %s\n", path.c_str());
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

/**
 * @brief Copy a config file within LittleFS.
 *        Body: {"from":"<src>","to":"<dst>"}. Rejects dst == config.json
 *        (use /api/config/activate instead).
 */
void DeviceApi::_onCopyConfig() {
  LOG_PRINTLN(F("API: POST /api/config/copy"));
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) ||
      !doc[kFrom].is<const char *>() || !doc[kTo].is<const char *>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"expected {from, to}\"}"));
    return;
  }
  String fromName = doc[kFrom].as<const char *>();
  String toName = doc[kTo].as<const char *>();
  if (!_sanitizeCfgName(fromName) || !_sanitizeCfgName(toName)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  if (toName == "config.json") {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"use /api/config/activate instead\"}"));
    return;
  }
  String fromPath = "/" + fromName;
  String toPath = "/" + toName;
  if (!LittleFS.exists(fromPath.c_str())) {
    ApiServer::sendJson(kNotFound, F("{\"error\":\"source not found\"}"));
    return;
  }
  if (!_copyFile(fromPath.c_str(), toPath.c_str())) {
    ApiServer::sendJson(kInternalError, F("{\"error\":\"write failed\"}"));
    return;
  }
  LOG_PRINTF("[FS] copied %s -> %s\n", fromPath.c_str(), toPath.c_str());
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

/**
 * @brief Rename a config file within LittleFS (copy-then-delete).
 *        Body: {"from":"<old>","to":"<new>"}. Rejects renaming config.json.
 */
void DeviceApi::_onRenameConfig() {
  LOG_PRINTLN(F("API: POST /api/config/rename"));
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) ||
      !doc[kFrom].is<const char *>() || !doc[kTo].is<const char *>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"expected {from, to}\"}"));
    return;
  }
  String fromName = doc[kFrom].as<const char *>();
  String toName = doc[kTo].as<const char *>();
  if (!_sanitizeCfgName(fromName) || !_sanitizeCfgName(toName)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid filename\"}"));
    return;
  }
  if (fromName == "config.json" || toName == "config.json") {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"cannot rename active config\"}"));
    return;
  }
  String fromPath = "/" + fromName;
  String toPath = "/" + toName;
  if (!LittleFS.exists(fromPath.c_str())) {
    ApiServer::sendJson(kNotFound, F("{\"error\":\"source not found\"}"));
    return;
  }
  LOG_PRINTF("[rename] %s -> %s\n", fromPath.c_str(), toPath.c_str());
  if (!_safeRename(fromPath.c_str(), toPath.c_str())) {
    ApiServer::sendJson(kInternalError, F("{\"error\":\"rename failed\"}"));
    return;
  }
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

/**
 * @brief Copy a named file to config.json, making it the active config.
 *        Body: {"file":"<filename>"}. Records the source in /config_source.txt.
 */
void DeviceApi::_onActivateConfig() {
  LOG_PRINTLN(F("API: POST /api/config/activate"));
  if (!ApiServer::server().hasArg(kArgPlain)) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"body required\"}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, ApiServer::server().arg(kArgPlain)) || !doc[kFile].is<const char *>()) {
    ApiServer::sendJson(kBadRequest, F("{\"error\":\"invalid JSON\"}"));
    return;
  }
  String file = "/";
  file += doc[kFile].as<const char *>();
  if (!LittleFS.exists(file.c_str())) {
    ApiServer::sendJson(kNotFound, F("{\"error\":\"file not found\"}"));
    return;
  }
  if (!ConfigManager::activateConfig(file.c_str())) {
    ApiServer::sendJson(kInternalError, F("{\"error\":\"copy failed\"}"));
    return;
  }
  ApiServer::sendJson(kOk, F("{\"ok\":true}"));
}

#endif // MRJFX_API_SERVER_ENABLED
