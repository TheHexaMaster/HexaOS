/*
  HexaOS - network_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Network domain handler for HexaOS.
  Owns the WiFi connection state machine, automatic retry policy and the
  public network API consumed by the command layer and other modules.
  Delegates physical WiFi operations to wifi_adapter; callers never touch
  the adapter directly.
  Config persistence: wifi.ssid / wifi.password are read on auto-connect
  and written back when NetworkConnect() is called with new credentials.
  Gated by HX_ENABLE_MODULE_NETWORK.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_NETWORK

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

typedef enum : uint8_t {
  HX_NETWORK_STATE_IDLE,        // No connection attempt pending
  HX_NETWORK_STATE_CONNECTING,  // connect() issued, waiting for adapter event
  HX_NETWORK_STATE_CONNECTED,   // STA associated and IP acquired
  HX_NETWORK_STATE_FAILED,      // Last attempt failed; retry pending
} HxNetworkState;

// ---------------------------------------------------------------------------
// Events emitted to subscribers
// ---------------------------------------------------------------------------

typedef enum : uint8_t {
  HX_NETWORK_EVENT_CONNECTED,      // State machine entered CONNECTED
  HX_NETWORK_EVENT_DISCONNECTED,   // Association lost
  HX_NETWORK_EVENT_IP_ACQUIRED,    // DHCP lease obtained
  HX_NETWORK_EVENT_IP_LOST,        // IP address lost
  HX_NETWORK_EVENT_CONNECT_FAILED, // All retry attempts exhausted
} HxNetworkEvent;

typedef void (*HxNetworkEventCb)(HxNetworkEvent event, void* user);

// ---------------------------------------------------------------------------
// Lifecycle — called by mod_network
// ---------------------------------------------------------------------------

bool NetworkInit();
void NetworkStart();
void NetworkEverySecond();

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

// Connect to an AP and save credentials to config.
// Resets the retry counter so automatic retry uses the new credentials.
bool NetworkConnect(const char* ssid, const char* password);

// Disconnect and stop retry attempts. Does not clear saved credentials.
bool NetworkDisconnect();

// Connect using credentials stored in wifi.ssid / wifi.password config keys.
// Returns false when no SSID is saved.
bool NetworkAutoConnect();

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

HxNetworkState NetworkGetState();
const char*    NetworkStateStr(HxNetworkState state);
bool           NetworkIsConnected();
bool           NetworkGetIp(char* out, size_t out_size);
int8_t         NetworkGetRssi();

// ---------------------------------------------------------------------------
// Event subscription (single subscriber)
// ---------------------------------------------------------------------------

void NetworkSetEventCallback(HxNetworkEventCb cb, void* user);

#endif // HX_ENABLE_MODULE_NETWORK
