/*
  HexaOS - esp_fs.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  ESP filesystem platform adapter.
  Wraps LittleFS mount logic behind a narrow HexaOS platform function so higher-level services remain decoupled from raw framework calls.
*/

#include "hexaos.h"
#include <FS.h>
#include <LittleFS.h>

bool EspLittlefsMount() {
  if (LittleFS.begin(true, "", 10, "littlefs")) {
    LogInfo("LittleFS mount OK");
    return true;
  }

  LogError("LittleFS mount failed");
  return false;
}
