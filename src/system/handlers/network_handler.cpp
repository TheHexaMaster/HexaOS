/*
  HexaOS - network_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Network domain handler implementation.

  State machine transitions:
    IDLE        → NetworkConnect() issued                  → CONNECTING
    CONNECTING  → adapter IP_ACQUIRED                      → CONNECTED
    CONNECTING  → adapter DISCONNECTED                     → FAILED (retry armed)
    CONNECTED   → adapter DISCONNECTED                     → FAILED (retry armed)
    FAILED      → retry interval elapsed, retry < max      → CONNECTING
    FAILED      → retry count == HX_WIFI_RETRY_MAX         → stays FAILED, no more retries
    any         → NetworkDisconnect() called               → IDLE

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

#include "system/adapters/wifi_adapter.h"
#include "system/core/config.h"
#include "system/core/log.h"
#include "system/core/runtime.h"
#include "system/core/scheduler.h"

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static constexpr const char* HX_NET_TAG = "NET";

static HxNetworkState   g_state          = HX_NETWORK_STATE_IDLE;
static int              g_retry_count    = 0;
static HxScheduler      g_retry_sched;
static HxNetworkEventCb g_event_cb       = nullptr;
static void*            g_event_cb_user  = nullptr;

static char g_ssid[64];
static char g_password[64];

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void FireEvent(HxNetworkEvent event) {
  if (g_event_cb) {
    g_event_cb(event, g_event_cb_user);
  }
}

static void OnWifiAdapterEvent(WifiAdapterEvent event, void* user) {
  (void)user;

  switch (event) {
    case WIFI_ADAPTER_EVENT_CONNECTED:
      // STA associated; wait for IP_ACQUIRED before marking fully connected.
      break;

    case WIFI_ADAPTER_EVENT_IP_ACQUIRED:
      g_state       = HX_NETWORK_STATE_CONNECTED;
      g_retry_count = 0;
      HX_LOGI(HX_NET_TAG, "connected");
      FireEvent(HX_NETWORK_EVENT_IP_ACQUIRED);
      FireEvent(HX_NETWORK_EVENT_CONNECTED);
      break;

    case WIFI_ADAPTER_EVENT_IP_LOST:
      FireEvent(HX_NETWORK_EVENT_IP_LOST);
      break;

    case WIFI_ADAPTER_EVENT_DISCONNECTED:
      if (g_state == HX_NETWORK_STATE_CONNECTED ||
          g_state == HX_NETWORK_STATE_CONNECTING) {
        g_retry_count++;
        if (g_retry_count >= HX_WIFI_RETRY_MAX) {
          g_state = HX_NETWORK_STATE_FAILED;
          HX_LOGW(HX_NET_TAG, "max retries reached (%d), giving up", g_retry_count);
          FireEvent(HX_NETWORK_EVENT_CONNECT_FAILED);
        } else {
          g_state = HX_NETWORK_STATE_FAILED;
          HxSchedulerReset(&g_retry_sched);
          HX_LOGW(HX_NET_TAG, "disconnected — retry %d/%d in %d ms",
                  g_retry_count, HX_WIFI_RETRY_MAX, HX_WIFI_RETRY_INTERVAL_MS);
          FireEvent(HX_NETWORK_EVENT_DISCONNECTED);
        }
      }
      break;
  }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool NetworkInit() {
  memset(g_ssid,     0, sizeof(g_ssid));
  memset(g_password, 0, sizeof(g_password));
  HxSchedulerInit(&g_retry_sched, HX_WIFI_RETRY_INTERVAL_MS, 0);
  HxSchedulerDisable(&g_retry_sched);
  return WifiAdapterInit();
}

void NetworkStart() {
  WifiAdapterSetEventCallback(OnWifiAdapterEvent, nullptr);
  NetworkAutoConnect();
}

void NetworkEverySecond() {
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
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

bool NetworkConnect(const char* ssid, const char* password) {
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
}

bool NetworkDisconnect() {
  g_state       = HX_NETWORK_STATE_IDLE;
  g_retry_count = 0;
  HxSchedulerDisable(&g_retry_sched);
  return WifiAdapterDisconnect();
}

bool NetworkAutoConnect() {
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
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

HxNetworkState NetworkGetState() {
  return g_state;
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
  return g_state == HX_NETWORK_STATE_CONNECTED;
}

bool NetworkGetIp(char* out, size_t out_size) {
  return WifiAdapterGetIp(out, out_size);
}

int8_t NetworkGetRssi() {
  return WifiAdapterGetRssi();
}

// ---------------------------------------------------------------------------
// Event subscription
// ---------------------------------------------------------------------------

void NetworkSetEventCallback(HxNetworkEventCb cb, void* user) {
  g_event_cb      = cb;
  g_event_cb_user = user;
}

#endif // HX_ENABLE_MODULE_NETWORK
