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
#include "system/core/config.h"
#include "system/core/pinmap.h"
#include "system/core/state.h"
#include "system/services/time_sync_service.h"

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
  HX_LOGI("BOOT", "%s boot start", HX_SYSTEM_NAME);
  HX_LOGI("BOOT", "Version: %s", HX_VERSION);
  HX_LOGI("BOOT", "Board:   %s", HX_TARGET_NAME);
  LogRaw("========================================");
}

static void BootPrintResetInfo() {
  uint32_t reason = (uint32_t)esp_reset_reason();
  HX_LOGI("BOOT", "Reset reason: %s (%lu)", EspResetReasonText(reason), (unsigned long)reason);
}

static void BootPrintChipInfo() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  HX_LOGI("BOOT", "Chip model: %s", CONFIG_IDF_TARGET);
  HX_LOGI("BOOT", "Chip rev:   %d", info.revision);
  HX_LOGI("BOOT", "CPU cores:  %d", info.cores);
  HX_LOGI("BOOT", "Flash size: %u KB", ESP.getFlashChipSize() / 1024U);
}

void BootInit() {
  Hx.rtos_ready = RtosInit();

  LogInit();

  if (!UserInterfaceInit()) {
    HX_LOGW("UI", "init failed");
  }

  Hx.time_ready = TimeInit();
  if (!Hx.time_ready) {
    HX_LOGW("TIM", "init failed");
  } else {
    HX_LOGI("TIM", "init OK");
  }

  if (!Hx.rtos_ready) {
    HX_LOGE("RTS", "init failed");
  } else {
    HX_LOGI("RTS", "init OK");
  }

  BootPrintBanner();
  BootPrintResetInfo();
  BootPrintChipInfo();

  if (!ConfigInit()) {
    HX_LOGW("CFG", "init failed");
  }

  if (!ConfigLoad()) {
    HX_LOGW("CFG", "load failed");
  }

  ConfigApply();

  if (!PinmapInit()) {
    HX_LOGW("PIN", "init failed");
  }

  TimeSyncBootTryRtc();

  if (!StateInit()) {
    HX_LOGW("STA", "init failed");
  }

  if (!StateLoad()) {
    HX_LOGW("STA", "load failed");
  }

  if (!CommandInit()) {
    HX_LOGW("CMD", "init failed");
  }

  UserInterfaceStart();
}
