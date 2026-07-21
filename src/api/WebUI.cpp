/**
 * @file WebUI.cpp
 *
 * @project MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */

#include "api/WebUI.h"

#ifdef LFX_WEBUI_ENABLED

  #include "api/ApiServer.h"
  #include "generated/webui_html.h"

static void _onGetRoot() {
  ApiServer::server().sendHeader("Location", "/ui");
  ApiServer::server().send(302);
}

void WebUI::init() {
  ApiServer::on("/",   HTTP_GET, _onGetRoot);
  ApiServer::on("/ui", HTTP_GET, _onGetUi);
}

void WebUI::_onGetUi() {
  ApiServer::server().sendHeader("Cache-Control", "no-store");
  ApiServer::server().sendHeader("Content-Encoding", "gzip");
  ApiServer::server().send_P(200, "text/html",
    (const char *)WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
}

#endif  // LFX_WEBUI_ENABLED
