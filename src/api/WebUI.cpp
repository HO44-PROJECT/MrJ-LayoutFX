/**
 * @file WebUI.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/WebUI.h"

#ifdef MRJFX_WEBUI_ENABLED

  #include "api/ApiServer.h"
  #include "api/webui_html.h"

void WebUI::init() {
  ApiServer::on("/ui", HTTP_GET, _onGetUi);
}

void WebUI::_onGetUi() {
  ApiServer::server().sendHeader("Content-Encoding", "gzip");
  ApiServer::server().send_P(200, "text/html",
    (const char *)WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
}

#endif  // MRJFX_WEBUI_ENABLED
