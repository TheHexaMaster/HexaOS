/*
  HexaOS - system_loop.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core runtime service loop implementation for HexaOS.
  Owns the mandatory per-iteration system work that must always run even when
  optional modules are disabled, including user interface polling, state
  service maintenance, uptime tracking and the built-in heartbeat log tick.
*/

#include "system_loop.h"

#include <Arduino.h>

#include "system/core/log.h"
#include "system/core/runtime.h"
#include "system/core/time.h"
#include "system/core/user_interface.h"
#include "system/core/state.h"

void SystemLoop() {
  Hx.uptime_ms = millis();

  UserInterfaceLoop();
  StateLoop();
}

void SystemEvery100ms() {
}

void HeartBeatTick() {
  char uptime_text[32];
  TimeFormatMonotonic(uptime_text, sizeof(uptime_text), (uint64_t)Hx.uptime_ms);

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

void SystemEverySecond() {
//  HeartBeatTick();
}
