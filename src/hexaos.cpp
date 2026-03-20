/*
  HexaOS - hexaos.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Main Arduino entry point for HexaOS.
  Coordinates core initialization, service startup, filesystem mounting, module startup and the main cooperative runtime loop executed by the firmware.
*/

#include <Arduino.h>

#include "headers/hx_build.h"
#include "system/commands/command_engine.h"
#include "system/core/log.h"
#include "system/core/module_registry.h"
#include "system/core/rtos.h"
#include "system/core/runtime.h"
#include "system/core/user_interface.h"
#include "system/handlers/littlefs_handler.h"
#include "system/handlers/nvs_config_handler.h"
#include "system/handlers/nvs_state_handler.h"

#include <esp_system.h>
#include "esp_chip_info.h"


static const char* EspResetReasonText(uint32_t reason) {
  switch ((esp_reset_reason_t)reason) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    default:                return "UNKNOWN";
  }
}



static void BootPrintBanner() {
  LogRaw("========================================");
  LogInfo("%s boot start", HX_SYSTEM_NAME);
  LogInfo("Version: %s", HX_VERSION);
  LogInfo("Board:   %s", HX_TARGET_NAME);
  LogRaw("========================================");
}

static void BootPrintResetInfo() {
  uint32_t reason = (uint32_t)esp_reset_reason();
  LogInfo("Reset reason: %s (%lu)", EspResetReasonText(reason), (unsigned long)reason);
}

static void BootPrintChipInfo() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  LogInfo("Chip model: %s", CONFIG_IDF_TARGET);
  LogInfo("Chip rev:   %d", info.revision);
  LogInfo("CPU cores:  %d", info.cores);
  LogInfo("Flash size: %u KB", ESP.getFlashChipSize() / 1024);
}

static void BootInit() {
  BootPrintBanner();
  BootPrintResetInfo();
  BootPrintChipInfo();

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

  if (!CommandInit()) {
    LogWarn("CMD: init failed");
  }

  UserInterfaceStart();

  ModuleInitAll();
  ModuleStartAll();
}



void setup() {

#if HX_ENABLE_CORE_RTOS
  Hx.rtos_ready = RtosInit();
#else
  Hx.rtos_ready = false;
#endif

  // We need log system first for debug
  LogInit();

  if (!UserInterfaceInit()) {
    LogWarn("UI: init failed");
  }


#if HX_ENABLE_CORE_RTOS
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

  UserInterfaceLoop();

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