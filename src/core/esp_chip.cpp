/*
  HexaOS - esp_chip.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  ESP platform information helper.
  Reads chip metadata from the ESP-IDF and Arduino runtime and prints a compact hardware summary during startup for diagnostics and support.
*/

#include "hexaos.h"
#include "esp_chip_info.h"

void EspPrintChipInfo() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  LogInfo("Chip model: %s", CONFIG_IDF_TARGET);
  LogInfo("Chip rev:   %d", info.revision);
  LogInfo("CPU cores:  %d", info.cores);
  LogInfo("Flash size: %u KB", ESP.getFlashChipSize() / 1024);
}
