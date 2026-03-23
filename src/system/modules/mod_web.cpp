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
#include <ArduinoJson.h>
#include "esp_timer.h"
#include "headers/hx_pinfunc.h"
#include "headers/hx_target_caps.h"
#include "system/core/config.h"
#include "system/core/pinmap.h"
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

// Human-readable constraint description for a config key, used in error responses.
static void WebConfigConstraintDescription(const HxConfigKeyDef* k, char* out, size_t out_size) {
  switch (k->type) {
    case HX_SCHEMA_VALUE_BOOL:
      snprintf(out, out_size, "Must be 'true' or 'false'");
      break;
    case HX_SCHEMA_VALUE_INT32:
      snprintf(out, out_size, "Must be an integer from %ld to %ld", (long)k->min_i32, (long)k->max_i32);
      break;
    case HX_SCHEMA_VALUE_FLOAT:
      snprintf(out, out_size, "Must be a number from %.4g to %.4g", (double)k->min_f32, (double)k->max_f32);
      break;
    case HX_SCHEMA_VALUE_STRING:
      snprintf(out, out_size, "Must be at most %u characters", (unsigned)k->max_len);
      break;
    default:
      snprintf(out, out_size, "Invalid value");
      break;
  }
}

// Schedule esp_restart() via a one-shot timer so the HTTP response is
// transmitted before the chip resets (500 ms is sufficient for lwIP flush).
static void WebScheduleRestart() {
  esp_timer_handle_t timer;
  esp_timer_create_args_t args = {};
  args.callback = [](void*) { esp_restart(); };
  args.name     = "web_rst";
  if (esp_timer_create(&args, &timer) == ESP_OK) {
    esp_timer_start_once(timer, 500000); // 500 ms in microseconds
  }
}

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

  // GET /api/config — all console-visible config keys (large read-only strings excluded)
  g_server->on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    char* buf = (char*)malloc(4096);
    if (!buf) { request->send(503, "application/json", "{\"error\":\"oom\"}"); return; }
    size_t pos  = 0;
    size_t n    = ConfigKeyCount();
    pos += snprintf(buf + pos, 4096 - pos, "[");
    bool first = true;
    for (size_t i = 0; i < n; i++) {
      const HxConfigKeyDef* k = ConfigKeyAt(i);
      if (!k->console_visible) continue;
      if (k->type == HX_SCHEMA_VALUE_STRING && k->max_len > 128) continue;
      char val[130];
      if (!ConfigValueToString(k, val, sizeof(val))) val[0] = '\0';
      // JSON-escape the value (handle quotes and backslashes)
      char esc[260];
      size_t ei = 0;
      for (const char* p = val; *p && ei < sizeof(esc) - 2; p++) {
        if (*p == '"' || *p == '\\') esc[ei++] = '\\';
        esc[ei++] = *p;
      }
      esc[ei] = '\0';
      const char* type_str =
        k->type == HX_SCHEMA_VALUE_BOOL  ? "bool"   :
        k->type == HX_SCHEMA_VALUE_INT32 ? "int"    :
        k->type == HX_SCHEMA_VALUE_FLOAT ? "float"  : "string";
      pos += snprintf(buf + pos, 4096 - pos,
        "%s{\"key\":\"%s\",\"value\":\"%s\",\"type\":\"%s\",\"writable\":%s}",
        first ? "" : ",", k->key, esc, type_str,
        k->console_writable ? "true" : "false");
      first = false;
    }
    pos += snprintf(buf + pos, 4096 - pos, "]");
    buf[pos] = '\0';
    request->send(200, "application/json", buf);
    free(buf);
  });

  // POST /api/config — set a single key; body: key=...&value=...
  g_server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("key", true) || !request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing params\"}");
      return;
    }
    String key_str   = request->getParam("key",   true)->value();
    String value_str = request->getParam("value", true)->value();
    const HxConfigKeyDef* k = ConfigFindConfigKey(key_str.c_str());
    if (!k) {
      request->send(404, "application/json", "{\"ok\":false,\"error\":\"key not found\"}");
      return;
    }
    if (!k->console_writable) {
      request->send(403, "application/json", "{\"ok\":false,\"error\":\"read only\"}");
      return;
    }
    if (!ConfigSetValueFromString(k, value_str.c_str())) {
      char detail[128];
      WebConfigConstraintDescription(k, detail, sizeof(detail));
      char resp[192];
      snprintf(resp, sizeof(resp), "{\"ok\":false,\"error\":\"%s\"}", detail);
      request->send(400, "application/json", resp);
      return;
    }
    ConfigSave();
    ConfigApply();
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // GET /api/pinmap
  // Response:
  //   gpio_count  - total GPIOs on target (table rows)
  //   writable    - whether board.pinmap can be changed at runtime
  //   gpios[]     - dense array, index=GPIO number, value=HxPinFunction ID (0=none)
  //   defaults[]  - dense array, index=GPIO number, value=build-default HxPinFunction ID
  //   reserved[]  - GPIO indices that are not user-assignable (flash/PSRAM/invalid)
  //   functions[] - all available {func, name} pairs for the select options
  g_server->on("/api/pinmap", HTTP_GET, [](AsyncWebServerRequest* request) {
    uint8_t gpio_count = PinmapGpioCount();
    char* buf = (char*)malloc(10240);
    if (!buf) { request->send(503, "application/json", "{\"error\":\"oom\"}"); return; }
    size_t pos = 0;
    const size_t bufsz = 10240;

    const HxConfigKeyDef* pm_key = ConfigFindConfigKey("board.pinmap");
    bool pm_writable = pm_key && pm_key->console_writable;

    // gpio_count + writable
    pos += snprintf(buf + pos, bufsz - pos,
      "{\"gpio_count\":%u,\"writable\":%s,", (unsigned)gpio_count, pm_writable ? "true" : "false");

    // gpios — dense array: index=GPIO, value=current function ID
    pos += snprintf(buf + pos, bufsz - pos, "\"gpios\":[");
    for (uint8_t gpio = 0; gpio < gpio_count; gpio++) {
      uint16_t func = 0;
      PinmapGetFunctionForGpio(gpio, &func);
      pos += snprintf(buf + pos, bufsz - pos, "%s%u", gpio ? "," : "", (unsigned)func);
    }
    pos += snprintf(buf + pos, bufsz - pos, "],");

    // defaults — build-time default function IDs per GPIO
    pos += snprintf(buf + pos, bufsz - pos, "\"defaults\":[");
    if (pm_key) {
      char def_str[HX_BUILD_BOARD_PINMAP_MAX_LEN + 1];
      if (ConfigDefaultToString(pm_key, def_str, sizeof(def_str))) {
        JsonDocument def_doc;
        if (deserializeJson(def_doc, def_str) == DeserializationError::Ok && def_doc.is<JsonArray>()) {
          JsonArray def_arr = def_doc.as<JsonArray>();
          uint8_t di = 0;
          for (JsonVariant v : def_arr) {
            if (di >= gpio_count) break;
            pos += snprintf(buf + pos, bufsz - pos, "%s%u", di ? "," : "", (unsigned)v.as<uint16_t>());
            di++;
          }
          for (; di < gpio_count; di++) {
            pos += snprintf(buf + pos, bufsz - pos, "%s0", di ? "," : "");
          }
        }
      }
    }
    pos += snprintf(buf + pos, bufsz - pos, "],");

    // reserved — GPIOs the user must not modify
    pos += snprintf(buf + pos, bufsz - pos, "\"reserved\":[");
    bool first = true;
    for (uint8_t gpio = 0; gpio < gpio_count; gpio++) {
      uint16_t caps = PinmapGetGpioCaps(gpio);
      bool r = !(caps & HX_GPIO_CAP_VALID) ||
                (caps & HX_GPIO_CAP_FLASH)  ||
                (caps & HX_GPIO_CAP_PSRAM);
      if (!r) continue;
      pos += snprintf(buf + pos, bufsz - pos, "%s%u", first ? "" : ",", (unsigned)gpio);
      first = false;
    }
    pos += snprintf(buf + pos, bufsz - pos, "],");

    // functions — all assignable pin functions (excluding NONE and gap IDs)
    pos += snprintf(buf + pos, bufsz - pos, "\"functions\":[");
    first = true;
    for (uint16_t func = 1; func <= HX_PINFUNC_MAX_ID; func++) {
      const char* name = HxPinFunctionText(func);
      if (!name || strcmp(name, "UNKNOWN") == 0) continue;
      pos += snprintf(buf + pos, bufsz - pos,
        "%s{\"func\":%u,\"name\":\"%s\"}", first ? "" : ",", (unsigned)func, name);
      first = false;
    }
    pos += snprintf(buf + pos, bufsz - pos, "]}");
    buf[pos] = '\0';
    request->send(200, "application/json", buf);
    free(buf);
  });

  // POST /api/pinmap/reset — revert board.pinmap to build default and restart.
  g_server->on("/api/pinmap/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
    const HxConfigKeyDef* k = ConfigFindConfigKey("board.pinmap");
    if (!k) {
      request->send(500, "application/json", "{\"ok\":false,\"error\":\"key missing\"}");
      return;
    }
    if (!ConfigResetValue(k)) {
      HX_LOGE(HX_WEB_TAG, "POST /api/pinmap/reset: ConfigResetValue failed");
      request->send(500, "application/json", "{\"ok\":false,\"error\":\"reset failed\"}");
      return;
    }
    ConfigSave();
    request->send(200, "application/json", "{\"ok\":true}");
    WebScheduleRestart();
  });

  // POST /api/pinmap — save the full pinmap dense JSON array and restart.
  // Data is passed as a URL query parameter (?pinmap=[...]) so it is always
  // parsed by ESPAsyncWebServer regardless of body-handler registration.
  // ConfigApply is NOT called before restart; the boot sequence re-applies.
  g_server->on("/api/pinmap", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("pinmap")) {
      HX_LOGE(HX_WEB_TAG, "POST /api/pinmap: missing pinmap param");
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing pinmap\"}");
      return;
    }
    String pinmap_str = request->getParam("pinmap")->value();
    HX_LOGD(HX_WEB_TAG, "POST /api/pinmap: len=%u json=%s", (unsigned)pinmap_str.length(), pinmap_str.c_str());
    JsonDocument doc;
    DeserializationError derr = deserializeJson(doc, pinmap_str.c_str());
    if (derr != DeserializationError::Ok || !doc.is<JsonArray>()) {
      HX_LOGE(HX_WEB_TAG, "POST /api/pinmap: json parse error: %s", derr.c_str());
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }
    const HxConfigKeyDef* k = ConfigFindConfigKey("board.pinmap");
    if (!k) {
      HX_LOGE(HX_WEB_TAG, "POST /api/pinmap: board.pinmap key not found");
      request->send(500, "application/json", "{\"ok\":false,\"error\":\"key missing\"}");
      return;
    }
    // Re-serialize to canonical compact form for storage
    char new_pinmap[HX_BUILD_BOARD_PINMAP_MAX_LEN + 1];
    size_t written = serializeJson(doc, new_pinmap, sizeof(new_pinmap));
    if (written == 0 || written >= sizeof(new_pinmap)) {
      HX_LOGE(HX_WEB_TAG, "POST /api/pinmap: too large written=%u max=%u", (unsigned)written, (unsigned)HX_BUILD_BOARD_PINMAP_MAX_LEN);
      request->send(500, "application/json", "{\"ok\":false,\"error\":\"too large\"}");
      return;
    }
    new_pinmap[written] = '\0';
    if (!ConfigSetValueFromString(k, new_pinmap)) {
      HX_LOGE(HX_WEB_TAG, "POST /api/pinmap: ConfigSetValueFromString failed");
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"set failed\"}");
      return;
    }
    ConfigSave();
    request->send(200, "application/json", "{\"ok\":true}");
    WebScheduleRestart();
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
