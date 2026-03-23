/*
  HexaOS - network_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Network domain handler implementation.

  WiFi state machine transitions:
    IDLE        → NetworkConnect() issued              → CONNECTING
    CONNECTING  → WiFi IP_ACQUIRED                     → CONNECTED
    CONNECTING  → WiFi DISCONNECTED                    → FAILED (retry armed)
    CONNECTED   → WiFi DISCONNECTED                    → FAILED (retry armed)
    FAILED      → retry interval elapsed, retry < max  → CONNECTING
    FAILED      → retry count == HX_WIFI_RETRY_MAX     → stays FAILED, no more retries
    any         → NetworkDisconnect() called           → IDLE

  ETH handling (dual-active):
    ETH link and IP state are tracked independently via g_eth_has_ip.
    When ETH acquires an IP it is promoted to the IDF default netif, giving
    it routing priority over WiFi. When ETH loses its IP the WiFi STA netif
    is restored as default (if WiFi is up).
    EthAdapterInit() is called from NetworkInit() and is gated by
    HX_ENABLE_FEATURE_ETH so the rest of the logic is WiFi-only builds.

  Hx.net_* ownership:
    net_connected — true when any interface (WiFi or ETH) has an IP.
    net_has_ip    — same as net_connected (kept aligned for runtime query).
    Both are updated exclusively by this handler, never by the adapters.

  Retry policy:
    The retry counter increments on each DISCONNECTED event while in
    CONNECTING or CONNECTED state. When the counter reaches HX_WIFI_RETRY_MAX
    the handler stops retrying and fires HX_NETWORK_EVENT_CONNECT_FAILED.
    NetworkConnect() resets the counter, so a new explicit connect() always
    starts fresh.

  Config persistence:
    NetworkAutoConnect() reads wifi.ssid and wifi.password from the current
    in-memory config. NetworkConnect() writes back the credentials so they
    survive a reboot (caller must call ConfigSave() if persistence is needed,
    or call it from the command layer after a successful connect).
*/

#include "network_handler.h"

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_NETWORK

#include <string.h>

#include "esp_netif.h"

#include "system/adapters/wifi_adapter.h"
#if HX_ENABLE_FEATURE_ETH
#include "system/adapters/eth_adapter.h"
#endif
#include "system/core/config.h"
#include "system/core/log.h"
#include "system/core/runtime.h"
#include "system/core/scheduler.h"

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static constexpr const char* HX_NET_TAG = "NET";

#if HX_ENABLE_FEATURE_WIFI
static HxNetworkState   g_state          = HX_NETWORK_STATE_IDLE;
static int              g_retry_count    = 0;
static HxScheduler      g_retry_sched;
static char             g_ssid[64];
static char             g_password[64];
#endif

static HxNetworkEventCb g_event_cb       = nullptr;
static void*            g_event_cb_user  = nullptr;
static bool             g_eth_has_ip     = false;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void FireEvent(HxNetworkEvent event) {
  if (g_event_cb) {
    g_event_cb(event, g_event_cb_user);
  }
}

// Recompute Hx.net_* from the combined WiFi + ETH state.
// Must be called whenever either transport changes its IP state.
static void UpdateRuntimeFlags() {
#if HX_ENABLE_FEATURE_WIFI
  bool any_ip = (g_state == HX_NETWORK_STATE_CONNECTED) || g_eth_has_ip;
#else
  bool any_ip = g_eth_has_ip;
#endif
  Hx.net_connected = any_ip;
  Hx.net_has_ip    = any_ip;
}

#if HX_ENABLE_FEATURE_ETH
// Set ETH as the IDF default netif (highest routing priority).
static void EthSetDefault() {
  esp_netif_t* eth_netif = EthAdapterGetNetif();
  if (eth_netif) {
    esp_netif_set_default_netif(eth_netif);
    HX_LOGI(HX_NET_TAG, "ETH set as default netif");
  }
}

#if HX_ENABLE_FEATURE_WIFI
// Restore WiFi STA as the IDF default netif (ETH unavailable).
static void WifiSetDefault() {
  esp_netif_t* wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (wifi_netif) {
    esp_netif_set_default_netif(wifi_netif);
    HX_LOGI(HX_NET_TAG, "WiFi set as default netif");
  }
}
#endif // HX_ENABLE_FEATURE_WIFI
#endif // HX_ENABLE_FEATURE_ETH

#if HX_ENABLE_FEATURE_WIFI
static void OnWifiAdapterEvent(WifiAdapterEvent event, void* user) {
  (void)user;

  switch (event) {
    case WIFI_ADAPTER_EVENT_CONNECTED:
      // STA associated; wait for IP_ACQUIRED before marking fully connected.
      break;

    case WIFI_ADAPTER_EVENT_IP_ACQUIRED:
      g_state       = HX_NETWORK_STATE_CONNECTED;
      g_retry_count = 0;
      UpdateRuntimeFlags();
#if HX_ENABLE_FEATURE_ETH
      // ETH keeps priority if it already has an IP.
      if (!g_eth_has_ip) {
        WifiSetDefault();
      }
#endif
      HX_LOGI(HX_NET_TAG, "WiFi connected");
      FireEvent(HX_NETWORK_EVENT_IP_ACQUIRED);
      FireEvent(HX_NETWORK_EVENT_CONNECTED);
      break;

    case WIFI_ADAPTER_EVENT_IP_LOST:
      UpdateRuntimeFlags();
      FireEvent(HX_NETWORK_EVENT_IP_LOST);
      break;

    case WIFI_ADAPTER_EVENT_DISCONNECTED:
      if (g_state == HX_NETWORK_STATE_CONNECTED ||
          g_state == HX_NETWORK_STATE_CONNECTING) {
        g_retry_count++;
        if (g_retry_count >= HX_WIFI_RETRY_MAX) {
          g_state = HX_NETWORK_STATE_FAILED;
          UpdateRuntimeFlags();
          HX_LOGW(HX_NET_TAG, "WiFi max retries reached (%d), giving up", g_retry_count);
          FireEvent(HX_NETWORK_EVENT_CONNECT_FAILED);
        } else {
          g_state = HX_NETWORK_STATE_FAILED;
          UpdateRuntimeFlags();
          HxSchedulerReset(&g_retry_sched);
          HX_LOGW(HX_NET_TAG, "WiFi disconnected — retry %d/%d in %d ms",
                  g_retry_count, HX_WIFI_RETRY_MAX, HX_WIFI_RETRY_INTERVAL_MS);
          FireEvent(HX_NETWORK_EVENT_DISCONNECTED);
        }
      }
      break;
  }
}
#endif // HX_ENABLE_FEATURE_WIFI

#if HX_ENABLE_FEATURE_ETH
static void OnEthAdapterEvent(EthAdapterEvent event, void* user) {
  (void)user;

  switch (event) {
    case ETH_ADAPTER_EVENT_LINK_UP:
      // Wait for IP_ACQUIRED before updating flags.
      break;

    case ETH_ADAPTER_EVENT_IP_ACQUIRED:
      g_eth_has_ip = true;
      UpdateRuntimeFlags();
      EthSetDefault();
      HX_LOGI(HX_NET_TAG, "ETH connected (default netif)");
      FireEvent(HX_NETWORK_EVENT_IP_ACQUIRED);
      FireEvent(HX_NETWORK_EVENT_CONNECTED);
      break;

    case ETH_ADAPTER_EVENT_IP_LOST:
      g_eth_has_ip = false;
      UpdateRuntimeFlags();
      // Fall back to WiFi as default if it has an IP.
#if HX_ENABLE_FEATURE_WIFI
      if (g_state == HX_NETWORK_STATE_CONNECTED) {
        WifiSetDefault();
      }
#endif
      FireEvent(HX_NETWORK_EVENT_IP_LOST);
      break;

    case ETH_ADAPTER_EVENT_LINK_DOWN:
      g_eth_has_ip = false;
      UpdateRuntimeFlags();
#if HX_ENABLE_FEATURE_WIFI
      if (g_state == HX_NETWORK_STATE_CONNECTED) {
        WifiSetDefault();
      }
#endif
      FireEvent(HX_NETWORK_EVENT_DISCONNECTED);
      break;
  }
}
#endif // HX_ENABLE_FEATURE_ETH

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool NetworkInit() {
#if HX_ENABLE_FEATURE_WIFI
  memset(g_ssid,     0, sizeof(g_ssid));
  memset(g_password, 0, sizeof(g_password));
  HxSchedulerInit(&g_retry_sched, HX_WIFI_RETRY_INTERVAL_MS, 0);
  HxSchedulerDisable(&g_retry_sched);

  if (!WifiAdapterInit()) {
    return false;
  }
#endif

#if HX_ENABLE_FEATURE_ETH
  // Non-fatal: ETH failure does not prevent WiFi from working.
  if (!EthAdapterInit()) {
    HX_LOGW(HX_NET_TAG, "ETH init failed — continuing without Ethernet");
  }
#endif

  return true;
}

void NetworkStart() {
#if HX_ENABLE_FEATURE_WIFI
  WifiAdapterSetEventCallback(OnWifiAdapterEvent, nullptr);
#endif

#if HX_ENABLE_FEATURE_ETH
  EthAdapterSetEventCallback(OnEthAdapterEvent, nullptr);
#endif

  NetworkAutoConnect();
}

void NetworkEverySecond() {
#if HX_ENABLE_FEATURE_WIFI
  if (g_state != HX_NETWORK_STATE_FAILED) {
    return;
  }
  if (g_retry_count >= HX_WIFI_RETRY_MAX) {
    return;
  }
  if (g_ssid[0] == '\0') {
    return;
  }
  if (!HxSchedulerDue(&g_retry_sched)) {
    return;
  }

  HX_LOGI(HX_NET_TAG, "retry connect attempt %d", g_retry_count + 1);
  g_state = HX_NETWORK_STATE_CONNECTING;
  WifiAdapterConnect(g_ssid, g_password);
#endif
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

bool NetworkConnect(const char* ssid, const char* password) {
#if HX_ENABLE_FEATURE_WIFI
  if (!ssid || ssid[0] == '\0') {
    return false;
  }

  strncpy(g_ssid, ssid, sizeof(g_ssid) - 1);
  g_ssid[sizeof(g_ssid) - 1] = '\0';

  if (password) {
    strncpy(g_password, password, sizeof(g_password) - 1);
    g_password[sizeof(g_password) - 1] = '\0';
  } else {
    g_password[0] = '\0';
  }

  // Persist credentials to config so they survive reboot.
  const HxConfigKeyDef* ssid_key = ConfigFindConfigKey("wifi.ssid");
  const HxConfigKeyDef* pass_key = ConfigFindConfigKey("wifi.password");
  if (ssid_key) { ConfigSetValueFromString(ssid_key, g_ssid); }
  if (pass_key) { ConfigSetValueFromString(pass_key, g_password); }

  g_retry_count = 0;
  g_state       = HX_NETWORK_STATE_CONNECTING;
  HxSchedulerEnable(&g_retry_sched);

  return WifiAdapterConnect(g_ssid, g_password);
#else
  (void)ssid; (void)password;
  return false;
#endif
}

bool NetworkDisconnect() {
#if HX_ENABLE_FEATURE_WIFI
  g_state       = HX_NETWORK_STATE_IDLE;
  g_retry_count = 0;
  HxSchedulerDisable(&g_retry_sched);
  return WifiAdapterDisconnect();
#else
  return false;
#endif
}

bool NetworkAutoConnect() {
#if !HX_ENABLE_FEATURE_WIFI
  return false;
#else
  const HxConfigKeyDef* ssid_key = ConfigFindConfigKey("wifi.ssid");
  const HxConfigKeyDef* pass_key = ConfigFindConfigKey("wifi.password");

  if (!ssid_key || !pass_key) {
    HX_LOGW(HX_NET_TAG, "wifi config keys not found");
    return false;
  }

  char ssid[64];
  char password[64];

  if (!ConfigValueToString(ssid_key, ssid, sizeof(ssid)) || ssid[0] == '\0') {
    HX_LOGI(HX_NET_TAG, "no saved SSID — skipping auto-connect");
    return false;
  }

  ConfigValueToString(pass_key, password, sizeof(password));

  HX_LOGI(HX_NET_TAG, "auto-connecting to \"%s\"", ssid);
  return NetworkConnect(ssid, password);
#endif // HX_ENABLE_FEATURE_WIFI
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

HxNetworkState NetworkGetState() {
#if HX_ENABLE_FEATURE_WIFI
  return g_state;
#else
  return HX_NETWORK_STATE_IDLE;
#endif
}

const char* NetworkStateStr(HxNetworkState state) {
  switch (state) {
    case HX_NETWORK_STATE_IDLE:       return "idle";
    case HX_NETWORK_STATE_CONNECTING: return "connecting";
    case HX_NETWORK_STATE_CONNECTED:  return "connected";
    case HX_NETWORK_STATE_FAILED:     return "failed";
    default:                          return "unknown";
  }
}

bool NetworkIsConnected() {
#if HX_ENABLE_FEATURE_WIFI
  return (g_state == HX_NETWORK_STATE_CONNECTED) || g_eth_has_ip;
#else
  return g_eth_has_ip;
#endif
}

bool NetworkGetIp(char* out, size_t out_size) {
  // ETH has priority; fall back to WiFi.
#if HX_ENABLE_FEATURE_ETH
  if (g_eth_has_ip && EthAdapterGetIp(out, out_size)) {
    return true;
  }
#endif
#if HX_ENABLE_FEATURE_WIFI
  return WifiAdapterGetIp(out, out_size);
#else
  if (out && out_size > 0) { out[0] = '\0'; }
  return false;
#endif
}

int8_t NetworkGetRssi() {
#if HX_ENABLE_FEATURE_WIFI
  return WifiAdapterGetRssi();
#else
  return 0;
#endif
}

bool NetworkEthIsUp() {
#if HX_ENABLE_FEATURE_ETH
  return g_eth_has_ip;
#else
  return false;
#endif
}

bool NetworkEthGetIp(char* out, size_t out_size) {
#if HX_ENABLE_FEATURE_ETH
  return EthAdapterGetIp(out, out_size);
#else
  if (out && out_size > 0) { out[0] = '\0'; }
  return false;
#endif
}

// ---------------------------------------------------------------------------
// Event subscription
// ---------------------------------------------------------------------------

void NetworkSetEventCallback(HxNetworkEventCb cb, void* user) {
  g_event_cb      = cb;
  g_event_cb_user = user;
}

#endif // HX_ENABLE_MODULE_NETWORK
