/*
  HexaOS - hexaos.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Main Arduino entry point for HexaOS.
  Coordinates core initialization, service startup, filesystem mounting, module startup and the main cooperative runtime loop executed by the firmware.
*/

#include "hexaos.h"

void setup() {

#if HX_ENABLE_HANDLER_RTOS
  Hx.rtos_ready = RtosInit();
#else
  Hx.rtos_ready = false;
#endif

  // We need log system first for debug
  LogInit();

#if HX_ENABLE_HANDLER_RTOS
  if (!Hx.rtos_ready) {
    LogError("RTS: init failed");
  } else {
    HX_LOGI("RTS", "init OK");
  }
#endif

  // Standard boot procedure
  BootInit();


}

void loop() {
  static uint32_t last_100ms = 0;
  static uint32_t last_1s = 0;

  uint32_t now = millis();
  Hx.uptime_ms = now;

  ModuleLoopAll();
  StateLoop();

  if ((now - last_100ms) >= 100) {
    last_100ms = now;
    ModuleEvery100ms();
  }

  if ((now - last_1s) >= 1000) {
    last_1s = now;
    ModuleEverySecond();
  }
}