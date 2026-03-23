/*
  HexaOS - mod_web.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Web service module stub.
  Defines the lifecycle shell for future HTTP, web UI and remote service integration within the standard HexaOS module model.
*/

#include "headers/hx_build.h"
#include "system/core/log.h"
#include "system/core/module_registry.h"

#if HX_ENABLE_MODULE_WEB

static bool WebInit() {
  LogInfo("WEB: init");
  return true;
}

static void WebStart() {
  LogInfo("WEB: start");
}

static void WebLoop() {
}

static void WebEvery10ms() {
}

static void WebEvery100ms() {
}

static void WebEverySecond() {
}

const HxModule ModuleWeb = {
  .name = "web",
  .init = WebInit,
  .start = WebStart,
  .loop = WebLoop,
  .every_10ms = WebEvery10ms,
  .every_100ms = WebEvery100ms,
  .every_1s = WebEverySecond
};

#endif