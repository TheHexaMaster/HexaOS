/*
  HexaOS - mod_web.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Web service module.
  Hosts the ESPAsyncWebServer instance and registers HTTP routes directly.
  Gated by HX_ENABLE_MODULE_WEB.

  Lifecycle:
    WebInit  — instantiate AsyncWebServer, register routes.
    WebStart — call server.begin(); server accepts connections once an IP
               is acquired (lwIP is already up from network module init).
*/

#include "headers/hx_build.h"
#include "system/core/log.h"
#include "system/core/module_registry.h"

#if HX_ENABLE_MODULE_WEB

#include <ESPAsyncWebServer.h>

#include "system/web/assets/alpine_js_gz.h"
#include "system/web/pages/page_root.h"

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static constexpr const char* HX_WEB_TAG = "WEB";

static AsyncWebServer* g_server = nullptr;

// ---------------------------------------------------------------------------
// Routes
// ---------------------------------------------------------------------------

static void RegisterRoutes() {
  g_server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", PAGE_ROOT);
  });

  g_server->on("/assets/alpine.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "application/javascript", kAlpineJsGz, kAlpineJsGzLen);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    request->send(response);
  });

  g_server->onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
  });
}

// ---------------------------------------------------------------------------
// Module lifecycle
// ---------------------------------------------------------------------------

static bool WebInit() {
  g_server = new AsyncWebServer(80);
  if (!g_server) {
    HX_LOGE(HX_WEB_TAG, "server alloc failed");
    return false;
  }
  RegisterRoutes();
  HX_LOGI(HX_WEB_TAG, "init OK");
  return true;
}

static void WebStart() {
  g_server->begin();
  HX_LOGI(HX_WEB_TAG, "listening on port 80");
}

const HxModule ModuleWeb = {
  .name        = "web",
  .init        = WebInit,
  .start       = WebStart,
  .loop        = nullptr,
  .every_10ms  = nullptr,
  .every_100ms = nullptr,
  .every_1s    = nullptr
};

#endif // HX_ENABLE_MODULE_WEB
