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

  // We need log system first for debug
  LogInit();

  // Standard boot procedure
  BootInit();

  BootPrintBanner();
  BootPrintResetInfo();
  EspPrintChipInfo();

  if (!FactoryDataInit()) {
    LogWarn("FACT: init failed");
  }

  if (!ConfigInit()) {
    LogWarn("Config: init failed");
  }

  if (!ConfigLoad()) {
    LogWarn("Config: load failed");
  }

  ConfigApply();

  if (!StateInit()) {
    LogWarn("STA: init failed");
  }

  if (!FilesInit()) {
    LogWarn("FIL: init failed");
  }

  if (!StateLoad()) {
    LogWarn("STA: load failed");
  }

  if (!FilesMount()) {
    LogWarn("FIL: mount failed");
  }

  ModuleInitAll();
  ModuleStartAll();

#if HX_ENABLE_MODULE_CONSOLE
  ConsoleShowPrompt();
#endif
}

void loop() {
  static uint32_t last_100ms = 0;
  static uint32_t last_1s = 0;

  uint32_t now = millis();
  Hx.uptime_ms = now;

  ModuleLoopAll();

  if ((now - last_100ms) >= 100) {
    last_100ms = now;
    ModuleEvery100ms();
  }

  if ((now - last_1s) >= 1000) {
    last_1s = now;
    ModuleEverySecond();
  }
}