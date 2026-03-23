/*
  HexaOS - mod_web.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Web service module.
  Hosts the ESPAsyncWebServer instance and registers HTTP routes directly.
  Gated by HX_ENABLE_MODULE_WEB.

  Lifecycle:
    WebInit  — instantiate AsyncWebServer + AsyncWebSocket, register routes.
    WebStart — call server.begin(); server accepts connections once an IP
               is acquired (lwIP is already up from network module init).

  WebConsole (/ws/console):
    WebSocket endpoint bridged to the command engine via HxCmdOutput.
    Each text frame is treated as one command line. Output lines are sent
    back as individual text frames. Binary frames are ignored.
    The WS callback runs from the AsyncTCP task — CommandExecuteLine must
    be safe to call from a non-main task (it is read-only on the registry).
*/

#include "headers/hx_build.h"
#include "system/core/log.h"
#include "system/core/module_registry.h"

#if HX_ENABLE_MODULE_WEB

#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <stdio.h>
#include <string.h>

#include "system/commands/command_engine.h"
#include "system/core/log.h"
#include "system/core/runtime.h"
#include "system/web/assets/alpine_js_gz.h"
#include "system/web/pages/page_root.h"
#if HX_ENABLE_MODULE_NETWORK
#include "system/handlers/network_handler.h"
#endif

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static constexpr const char* HX_WEB_TAG = "WEB";

static AsyncWebServer*  g_server = nullptr;
static AsyncWebSocket*  g_ws     = nullptr;

// ---------------------------------------------------------------------------
// WebConsole — log broadcast
// ---------------------------------------------------------------------------

static void WsLogBroadcast(HxLogLevel /*level*/, const char* line, void* /*user*/) {
  if (!g_ws || !line || g_ws->count() == 0) {
    return;
  }
  // Send line + newline as a single frame — textAll copies data internally,
  // protected by std::recursive_mutex on ESP32, safe from any task.
  char buf[HX_LOG_LINE_MAX + 2];
  snprintf(buf, sizeof(buf), "%s\n", line);
  g_ws->textAll(buf);
}

static void WsSendHistory(AsyncWebSocketClient* client) {
  size_t used = LogHistorySize();
  if (used == 0) {
    return;
  }
  char* hist = (char*)malloc(used + 1);
  if (!hist) {
    return;
  }
  size_t copied = LogHistoryCopy(hist, used + 1);
  if (copied > 0) {
    client->text(hist, copied);
  }
  free(hist);
}

// ---------------------------------------------------------------------------
// WebConsole — HxCmdOutput bridge
// ---------------------------------------------------------------------------

struct WsOutputCtx {
  AsyncWebSocketClient* client;
};

static void WsWriteRaw(void* user, const char* text) {
  auto* ctx = static_cast<WsOutputCtx*>(user);
  if (ctx->client && ctx->client->canSend()) {
    ctx->client->text(text);
  }
}

static void WsWriteLine(void* user, const char* text) {
  auto* ctx = static_cast<WsOutputCtx*>(user);
  if (ctx->client && ctx->client->canSend()) {
    // Append newline so the terminal renders each line separately.
    char buf[HX_COMMAND_LINE_MAX + 2];
    snprintf(buf, sizeof(buf), "%s\n", text);
    ctx->client->text(buf);
  }
}

static void OnWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  (void)server;

  if (type == WS_EVT_CONNECT) {
    HX_LOGI(HX_WEB_TAG, "WS console: client #%u connected", client->id());
    WsSendHistory(client);
    client->text("--- console ready --- type 'help' for commands\n");

  } else if (type == WS_EVT_DISCONNECT) {
    HX_LOGI(HX_WEB_TAG, "WS console: client #%u disconnected", client->id());

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
    if (!info->final || info->index != 0 || info->len != len) {
      return;  // ignore fragmented frames
    }
    if (info->opcode != WS_TEXT) {
      return;  // ignore binary
    }

    // Null-terminate and strip trailing newline/CR.
    char line[HX_COMMAND_LINE_MAX];
    size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
    memcpy(line, data, copy);
    line[copy] = '\0';
    while (copy > 0 && (line[copy - 1] == '\n' || line[copy - 1] == '\r')) {
      line[--copy] = '\0';
    }

    WsOutputCtx ctx = { client };
    HxCmdOutput out = {
      .write_raw  = WsWriteRaw,
      .write_line = WsWriteLine,
      .user        = &ctx,
      .interactive = true
    };
    CommandExecuteLine(line, &out);
  }
}

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

  g_server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    uint32_t ms  = Hx.uptime_ms;
    uint32_t s   = ms / 1000;
    uint32_t sec = s % 60;
    uint32_t min = (s / 60) % 60;
    uint32_t hr  = (s / 3600) % 24;
    uint32_t day = s / 86400;

    char uptime[24];
    snprintf(uptime, sizeof(uptime), "%lud %luh %lum %lus",
             (unsigned long)day, (unsigned long)hr,
             (unsigned long)min, (unsigned long)sec);

    char ip[32] = "";
    int8_t rssi = 0;
    bool connected = false;
    bool eth_up    = false;

#if HX_ENABLE_MODULE_NETWORK
    connected = NetworkIsConnected();
    NetworkGetIp(ip, sizeof(ip));
    rssi   = NetworkGetRssi();
    eth_up = NetworkEthIsUp();
#endif

    char json[256];
    snprintf(json, sizeof(json),
             "{\"net_connected\":%s,"
             "\"ip\":\"%s\","
             "\"rssi\":%d,"
             "\"eth_up\":%s,"
             "\"uptime\":\"%s\"}",
             connected ? "true" : "false",
             ip,
             (int)rssi,
             eth_up ? "true" : "false",
             uptime);

    request->send(200, "application/json", json);
  });

  g_ws = new AsyncWebSocket("/ws/console");
  g_ws->onEvent(OnWsEvent);
  g_server->addHandler(g_ws);

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
  LogSetSecondaryLineCb(WsLogBroadcast, nullptr);
  HX_LOGI(HX_WEB_TAG, "init OK");
  return true;
}

static void WebStart() {
  g_server->begin();
  HX_LOGI(HX_WEB_TAG, "listening on port 80");
}

static void WebEvery1s() {
  // Clean up disconnected WebSocket clients to free memory.
  if (g_ws) {
    g_ws->cleanupClients();
  }
}

const HxModule ModuleWeb = {
  .name        = "web",
  .init        = WebInit,
  .start       = WebStart,
  .loop        = nullptr,
  .every_10ms  = nullptr,
  .every_100ms = nullptr,
  .every_1s    = WebEvery1s
};

#endif // HX_ENABLE_MODULE_WEB
