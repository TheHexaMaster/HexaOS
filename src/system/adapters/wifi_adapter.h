/*
  HexaOS - wifi_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  WiFi hardware adapter for HexaOS.
  Owns all interaction with the IDF WiFi stack (esp_wifi / esp_netif) and,
  when HX_ENABLE_FEATURE_ESP_HOSTED is set, the ESP-Hosted SDIO transport
  that makes a companion WiFi chip (e.g. C6/C2) transparent to esp_wifi on
  the host SoC (e.g. ESP32-P4).
  After WifiAdapterInit() succeeds all subsequent operations use the standard
  esp_wifi API regardless of which physical path is active.
  Gated by HX_ENABLE_FEATURE_WIFI.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_WIFI

#include <stddef.h>
#include <stdint.h>

// Events emitted to the registered callback.
// The callback is invoked from the IDF event-loop task — do not block in it.
typedef enum : uint8_t {
  WIFI_ADAPTER_EVENT_CONNECTED,    // STA associated with AP
  WIFI_ADAPTER_EVENT_DISCONNECTED, // STA lost association
  WIFI_ADAPTER_EVENT_IP_ACQUIRED,  // DHCP lease obtained
  WIFI_ADAPTER_EVENT_IP_LOST,      // IP address lost
} WifiAdapterEvent;

typedef void (*WifiAdapterEventCb)(WifiAdapterEvent event, void* user);

// Initialise the WiFi stack.
// When HX_ENABLE_FEATURE_ESP_HOSTED is set, resolves HOSTED0 SDIO pins from
// the pinmap and calls esp_hosted_init() before handing control to esp_wifi.
// Must be called once, before any other WifiAdapter* function.
bool WifiAdapterInit();

// Initiate a station connection. Stores config in esp_wifi; the result
// arrives asynchronously via the registered event callback.
bool WifiAdapterConnect(const char* ssid, const char* password);

// Disconnect from the current AP. Does not reset stored credentials.
bool WifiAdapterDisconnect();

// Returns true when the STA is associated with an AP (L2 connected).
bool WifiAdapterIsConnected();

// Returns true when a DHCP lease is held (L3 ready).
bool WifiAdapterHasIp();

// Writes the current IPv4 address into out as a dotted-decimal string.
// Returns false when no IP is held or on error.
bool WifiAdapterGetIp(char* out, size_t out_size);

// Returns the RSSI of the connected AP in dBm. Returns 0 when not connected.
int8_t WifiAdapterGetRssi();

// Register a single event subscriber. Replaces any previously registered one.
// Pass nullptr to unregister.
void WifiAdapterSetEventCallback(WifiAdapterEventCb cb, void* user);

#endif // HX_ENABLE_FEATURE_WIFI
