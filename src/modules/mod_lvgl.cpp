/*
  HexaOS - mod_lvgl.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  LVGL graphics module stub.
  Declares the lifecycle shell for future display and GUI integration so the graphics subsystem can be enabled as a normal HexaOS module.
*/

#include "hexaos.h"

#if HX_ENABLE_MODULE_LVGL

static bool LvglInit() {
  LogInfo("LVG: init");
  return true;
}

static void LvglStart() {
  LogInfo("LVG: start");
}

static void LvglLoop() {
}

static void LvglEvery100ms() {
}

static void LvglEverySecond() {
}

const HxModule ModuleLvgl = {
  .name = "lvgl",
  .init = LvglInit,
  .start = LvglStart,
  .loop = LvglLoop,
  .every_100ms = LvglEvery100ms,
  .every_1s = LvglEverySecond
};

#endif