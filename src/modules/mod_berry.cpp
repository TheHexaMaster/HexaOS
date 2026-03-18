/*
  HexaOS - mod_berry.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Berry scripting module stub.
  Provides the module shell for future Berry runtime integration and currently reserves the lifecycle entry points needed to attach the scripting subsystem cleanly.
*/

#include "hexaos.h"

#if HX_ENABLE_MODULE_BERRY

static bool BerryInit() {
  LogInfo("BRY: init");
  return true;
}

static void BerryStart() {
  LogInfo("BRY: start");
}

static void BerryLoop() {
}

static void BerryEvery100ms() {
}

static void BerryEverySecond() {
}

const HxModule ModuleBerry = {
  .name = "berry",
  .init = BerryInit,
  .start = BerryStart,
  .loop = BerryLoop,
  .every_100ms = BerryEvery100ms,
  .every_1s = BerryEverySecond
};

#endif