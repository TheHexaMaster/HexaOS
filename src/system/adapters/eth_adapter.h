/*
  HexaOS - eth_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Ethernet hardware adapter interface for HexaOS.
  Owns the EMAC + PHY driver lifecycle and netif glue.
  Callers receive link and IP events via the registered callback;
  they must not access the ETH driver directly.
  Gated by HX_ENABLE_FEATURE_ETH.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_ETH

#include <stddef.h>
#include <stdint.h>

#include "esp_netif.h"

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

typedef enum : uint8_t {
  ETH_ADAPTER_EVENT_LINK_UP,      // Physical link established (cable connected)
  ETH_ADAPTER_EVENT_LINK_DOWN,    // Physical link lost
  ETH_ADAPTER_EVENT_IP_ACQUIRED,  // DHCP lease obtained
  ETH_ADAPTER_EVENT_IP_LOST,      // IP address lost
} EthAdapterEvent;

typedef void (*EthAdapterEventCb)(EthAdapterEvent event, void* user);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Initialise EMAC + PHY, create ETH netif, register IDF event handlers,
// and start the ETH driver. Must be called after esp_netif_init() and
// esp_event_loop_create_default() (both handled by WifiAdapterInit).
bool EthAdapterInit();

bool         EthAdapterIsLinkUp();
bool         EthAdapterHasIp();
bool         EthAdapterGetIp(char* out, size_t out_size);
esp_netif_t* EthAdapterGetNetif();

void EthAdapterSetEventCallback(EthAdapterEventCb cb, void* user);

#endif // HX_ENABLE_FEATURE_ETH
