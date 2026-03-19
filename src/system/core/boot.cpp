/*
  HexaOS - boot.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Boot sequence helpers for early system startup.
  Provides boot banner output, reset reason decoding and other boot-time diagnostics used before the rest of the runtime is fully initialized.
*/

#include "hexaos.h"
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



void BootPrintBanner() {
  LogRaw("========================================");
  LogInfo("%s boot start", HX_SYSTEM_NAME);
  LogInfo("Version: %s", HX_VERSION);
  LogInfo("Board:   %s", HX_TARGET_NAME);
  LogRaw("========================================");
}

void BootPrintResetInfo() {
  uint32_t reason = (uint32_t)esp_reset_reason();
  LogInfo("Reset reason: %s (%lu)", EspResetReasonText(reason), (unsigned long)reason);
}

void BootPrintChipInfo() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  LogInfo("Chip model: %s", CONFIG_IDF_TARGET);
  LogInfo("Chip rev:   %d", info.revision);
  LogInfo("CPU cores:  %d", info.cores);
  LogInfo("Flash size: %u KB", ESP.getFlashChipSize() / 1024);
}

void BootInit() {

  BootPrintBanner();
  BootPrintResetInfo();
  BootPrintChipInfo();

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