/*
  HexaOS - mod_system.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core system status module.
  Provides basic system-level runtime reporting and periodic telemetry such as uptime and boot counters to validate that the scheduler and runtime are alive.
*/

#include "system/core/log.h"
#include "system/core/module_registry.h"
#include "system/core/runtime.h"

static bool SystemInit() {
  LogInfo("SYS: init");
  return true;
}

static void SystemStart() {
  LogInfo("SYS: start");
}

static void SystemLoop() {
}

static void SystemEvery100ms() {
}

static void SystemEverySecond() {

  HX_LOGI("SYS", " uptime=%lu ms boot_count=%lu",
          (unsigned long)Hx.uptime_ms,
          (unsigned long)Hx.boot_count);


}

const HxModule ModuleSystem = {
  .name = "system",
  .init = SystemInit,
  .start = SystemStart,
  .loop = SystemLoop,
  .every_100ms = SystemEvery100ms,
  .every_1s = SystemEverySecond
};