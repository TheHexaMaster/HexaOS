/*
  HexaOS - mod_network.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Network lifecycle module for HexaOS.
  Initialises and drives the network handler (WiFi, optional ESP-Hosted
  SDIO transport). Wires network state transitions into the runtime flags
  so other modules can observe connectivity without coupling directly to
  the handler.
  Gated by HX_ENABLE_MODULE_NETWORK.
*/

#include "system/core/log.h"
#include "system/core/module_registry.h"

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_NETWORK
  #include "system/handlers/network_handler.h"
#endif

static constexpr const char* HX_NET_MOD_TAG = "NET";

static bool NetworkModInit() {
#if HX_ENABLE_MODULE_NETWORK
  if (!NetworkInit()) {
    HX_LOGE(HX_NET_MOD_TAG, "init failed");
    return false;
  }
  return true;
#else
  return true;
#endif
}

static void NetworkModStart() {
#if HX_ENABLE_MODULE_NETWORK
  NetworkStart();
#endif
}

static void NetworkModLoop() {
}

static void NetworkModEvery10ms() {
}

static void NetworkModEvery100ms() {
}

static void NetworkModEverySecond() {
#if HX_ENABLE_MODULE_NETWORK
  NetworkEverySecond();
#endif
}

const HxModule ModuleNetwork = {
  .name        = "network",
  .init        = NetworkModInit,
  .start       = NetworkModStart,
  .loop        = NetworkModLoop,
  .every_10ms  = NetworkModEvery10ms,
  .every_100ms = NetworkModEvery100ms,
  .every_1s    = NetworkModEverySecond
};
