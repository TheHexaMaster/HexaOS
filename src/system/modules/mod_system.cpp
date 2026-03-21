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
#include "system/core/time.h"

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
  char uptime_text[32];
  TimeFormatMonotonic(uptime_text, sizeof(uptime_text), TimeMonotonicMs());

  if (!TimeIsSynchronized()) {
    HX_LOGI("SYS", "uptime=%s boot_count=%lu time=unsynced",
            uptime_text,
            (unsigned long)Hx.boot_count);
    return;
  }

  char utc_text[40];
  if (!TimeFormatNowUtc(utc_text, sizeof(utc_text))) {
    HX_LOGI("SYS", "uptime=%s boot_count=%lu time=sync-error",
            uptime_text,
            (unsigned long)Hx.boot_count);
    return;
  }

  HX_LOGI("SYS", "uptime=%s boot_count=%lu time=%s src=%s",
          uptime_text,
          (unsigned long)Hx.boot_count,
          utc_text,
          TimeSourceText(TimeGetSource()));
}

const HxModule ModuleSystem = {
  .name = "system",
  .init = SystemInit,
  .start = SystemStart,
  .loop = SystemLoop,
  .every_100ms = SystemEvery100ms,
  .every_1s = SystemEverySecond
};