/*
  HexaOS - wifi_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  WiFi hardware adapter implementation.

  Initialisation order (WifiAdapterInit):
    1. [HX_ENABLE_FEATURE_ESP_HOSTED] Resolve HOSTED0 SDIO pins from the
       pinmap, pass them to hostedSetPins(), then call hostedInitWiFi() from
       the Arduino HAL (esp32-hal-hosted.h). The HAL handles the full sequence:
       esp_hosted_sdio_set_config → esp_hosted_init → esp_hosted_connect_to_slave.
    2. esp_netif_init() — network interface layer.
    3. esp_event_loop_create_default() — IDF default event loop.
       ESP_ERR_INVALID_STATE is treated as success (already created).
    4. esp_netif_create_default_wifi_sta() — register STA netif.
    5. esp_wifi_init() — start the WiFi driver.
    6. Register WIFI_EVENT / IP_EVENT handlers.
    7. esp_wifi_set_mode(WIFI_MODE_STA) + esp_wifi_start().

  Event flow:
    IDF event loop → WifiEventHandler → updates g_connected / g_has_ip,
    fires g_event_cb. Hx.net_* flags are managed by network_handler.

  The registered event callback is called from the IDF event-loop task.
  Consumers must not block or call WifiAdapterConnect from within it.
*/

#include "wifi_adapter.h"

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_WIFI

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

#if HX_ENABLE_FEATURE_ESP_HOSTED
#include "esp32-hal-hosted.h"
#endif

#include <string.h>

#include "headers/hx_pinfunc.h"
#include "system/core/log.h"
#include "system/core/pinmap.h"

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static constexpr const char* HX_WIFI_TAG = "WIFI";

static bool               g_connected     = false;
static bool               g_has_ip        = false;
static WifiAdapterEventCb g_event_cb      = nullptr;
static void*              g_event_cb_user = nullptr;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void FireEvent(WifiAdapterEvent event) {
  if (g_event_cb) {
    g_event_cb(event, g_event_cb_user);
  }
}

static void WifiEventHandler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
  (void)arg;

  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_CONNECTED) {
      g_connected = true;
      HX_LOGI(HX_WIFI_TAG, "STA connected");
      FireEvent(WIFI_ADAPTER_EVENT_CONNECTED);

    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      g_connected = false;
      g_has_ip    = false;
      HX_LOGI(HX_WIFI_TAG, "STA disconnected");
      FireEvent(WIFI_ADAPTER_EVENT_DISCONNECTED);
    }

  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* ev = static_cast<ip_event_got_ip_t*>(event_data);
      g_has_ip = true;
      HX_LOGI(HX_WIFI_TAG, "IP acquired: " IPSTR, IP2STR(&ev->ip_info.ip));
      FireEvent(WIFI_ADAPTER_EVENT_IP_ACQUIRED);

    } else if (event_id == IP_EVENT_STA_LOST_IP) {
      g_has_ip = false;
      HX_LOGW(HX_WIFI_TAG, "IP lost");
      FireEvent(WIFI_ADAPTER_EVENT_IP_LOST);
    }
  }
}

// ---------------------------------------------------------------------------
// ESP-Hosted init (gated)
// ---------------------------------------------------------------------------

#if HX_ENABLE_FEATURE_ESP_HOSTED
static bool WifiAdapterInitHosted() {
  int clk   = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_CLK);
  int cmd   = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_CMD);
  int d0    = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D0);
  int d1    = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D1);
  int d2    = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D2);
  int d3    = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D3);
  int reset = PinmapGetGpioForFunction(HX_PIN_HOSTED0_RESET);

  if (clk < 0 || cmd < 0 || d0 < 0 || reset < 0) {
    HX_LOGE(HX_WIFI_TAG,
            "ESP-Hosted: mandatory SDIO pins not mapped (clk=%d cmd=%d d0=%d reset=%d)",
            clk, cmd, d0, reset);
    return false;
  }

  HX_LOGI(HX_WIFI_TAG,
           "ESP-Hosted SDIO: clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d reset=%d",
           clk, cmd, d0, d1, d2, d3, reset);

  // Delegate pin setup and full init sequence to the Arduino HAL.
  // hostedSetPins must be called before hostedInitWiFi.
  // hostedInitWiFi internally calls esp_hosted_sdio_set_config,
  // esp_hosted_init and esp_hosted_connect_to_slave.
  if (!hostedSetPins(
        static_cast<int8_t>(clk),
        static_cast<int8_t>(cmd),
        static_cast<int8_t>(d0),
        static_cast<int8_t>(d1),
        static_cast<int8_t>(d2),
        static_cast<int8_t>(d3),
        static_cast<int8_t>(reset))) {
    HX_LOGE(HX_WIFI_TAG, "ESP-Hosted: hostedSetPins failed");
    return false;
  }

  if (!hostedInitWiFi()) {
    HX_LOGE(HX_WIFI_TAG, "ESP-Hosted: hostedInitWiFi failed");
    return false;
  }

  HX_LOGI(HX_WIFI_TAG, "ESP-Hosted init OK");
  return true;
}
#endif // HX_ENABLE_FEATURE_ESP_HOSTED

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool WifiAdapterInit() {
#if HX_ENABLE_FEATURE_ESP_HOSTED
  if (!WifiAdapterInitHosted()) {
    return false;
  }
#endif

  esp_netif_init();

  esp_err_t loop_err = esp_event_loop_create_default();
  if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
    HX_LOGE(HX_WIFI_TAG, "event loop create failed: %s", esp_err_to_name(loop_err));
    return false;
  }

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    HX_LOGE(HX_WIFI_TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &WifiEventHandler, nullptr, nullptr);
  esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                      &WifiEventHandler, nullptr, nullptr);

  err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK) {
    HX_LOGE(HX_WIFI_TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_start();
  if (err != ESP_OK) {
    HX_LOGE(HX_WIFI_TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
    return false;
  }

  HX_LOGI(HX_WIFI_TAG, "init OK");
  return true;
}

bool WifiAdapterConnect(const char* ssid, const char* password) {
  if (!ssid || ssid[0] == '\0') {
    HX_LOGE(HX_WIFI_TAG, "connect: empty SSID");
    return false;
  }

  wifi_config_t wifi_cfg = {};
  strncpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid),
          ssid, sizeof(wifi_cfg.sta.ssid) - 1);
  if (password && password[0] != '\0') {
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.password),
            password, sizeof(wifi_cfg.sta.password) - 1);
  }

  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
  if (err != ESP_OK) {
    HX_LOGE(HX_WIFI_TAG, "set_config failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_wifi_connect();
  if (err != ESP_OK) {
    HX_LOGE(HX_WIFI_TAG, "connect failed: %s", esp_err_to_name(err));
    return false;
  }

  HX_LOGI(HX_WIFI_TAG, "connecting to \"%s\"", ssid);
  return true;
}

bool WifiAdapterDisconnect() {
  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK) {
    HX_LOGE(HX_WIFI_TAG, "disconnect failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool WifiAdapterIsConnected() {
  return g_connected;
}

bool WifiAdapterHasIp() {
  return g_has_ip;
}

bool WifiAdapterGetIp(char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return false;
  }
  if (!g_has_ip) {
    out[0] = '\0';
    return false;
  }

  esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!netif) {
    return false;
  }

  esp_netif_ip_info_t ip_info;
  if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
    return false;
  }

  snprintf(out, out_size, IPSTR, IP2STR(&ip_info.ip));
  return true;
}

int8_t WifiAdapterGetRssi() {
  wifi_ap_record_t ap_info = {};
  if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
    return 0;
  }
  return static_cast<int8_t>(ap_info.rssi);
}

void WifiAdapterSetEventCallback(WifiAdapterEventCb cb, void* user) {
  g_event_cb      = cb;
  g_event_cb_user = user;
}

#endif // HX_ENABLE_FEATURE_WIFI
