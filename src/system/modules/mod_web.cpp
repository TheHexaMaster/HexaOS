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

#include "system/core/runtime.h"

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
    request->send(200, "text/plain", HX_SYSTEM_NAME " v" HX_VERSION);
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
