/*
  HexaOS - boot.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core boot orchestration implementation for HexaOS.
  Owns the mandatory startup sequence for the firmware, including core service
  initialization, boot diagnostics, persistent configuration/state load,
  command engine startup and local user interface activation.
*/

#include "boot.h"

#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_system.h"

#include "headers/hx_build.h"
#include "system/commands/command_engine.h"
#include "system/core/log.h"
#include "system/core/rtos.h"
#include "system/core/runtime.h"
#include "system/core/time.h"
#include "system/core/user_interface.h"
#include "system/handlers/littlefs_handler.h"
#include "system/core/config.h"
#include "system/core/pinmap.h"
#include "system/core/state.h"

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
  LogInfo("Flash size: %u KB", ESP.getFlashChipSize() / 1024U);
}

void BootInit() {
  Hx.rtos_ready = RtosInit();

  LogInit();

  if (!UserInterfaceInit()) {
    LogWarn("UI: init failed");
  }

  Hx.time_ready = TimeInit();
  if (!Hx.time_ready) {
    LogWarn("TIM: init failed");
  } else {
    HX_LOGI("TIM", "init OK");
  }

  if (!Hx.rtos_ready) {
    LogError("RTS: init failed");
  } else {
    HX_LOGI("RTS", "init OK");
  }

  BootPrintBanner();
  BootPrintResetInfo();
  BootPrintChipInfo();

  if (!ConfigInit()) {
    LogWarn("CFG: init failed");
  }

  if (!ConfigLoad()) {
    LogWarn("CFG: load failed");
  }

  ConfigApply();

  if (!PinmapInit()) {
    LogWarn("PIN: init failed");
  }

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
}
