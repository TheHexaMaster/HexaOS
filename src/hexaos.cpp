/*
  HexaOS - hexaos.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Main Arduino entry point for HexaOS.
  Keeps the top-level firmware control flow intentionally minimal by delegating
  boot orchestration to the core boot service and the mandatory runtime service
  loop to the core system loop dispatcher.
*/

#include <Arduino.h>

#include "system/core/boot.h"
#include "system/core/module_registry.h"
#include "system/core/system_loop.h"
#include <ArduinoJson.h>

static void Every100ms() {
  SystemEvery100ms();
  ModuleEvery100ms();
}

static void EverySecond() {
  SystemEverySecond();
  ModuleEverySecond();
}

void setup() {
  BootInit();
  ModuleInitAll();
  ModuleStartAll();
}

void loop() {
  static uint32_t last_100ms = 0;
  static uint32_t last_1s = 0;

  uint32_t now = millis();

  SystemLoop();
  ModuleLoopAll();

  if ((uint32_t)(now - last_100ms) >= 100U) {
    last_100ms = now;
    Every100ms();
  }

  if ((uint32_t)(now - last_1s) >= 1000U) {
    last_1s = now;
    EverySecond();
  }
}
