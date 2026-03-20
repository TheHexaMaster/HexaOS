/*
  HexaOS - runtime.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Global runtime state storage.
  Creates the single Hx runtime instance that holds shared system flags, uptime and boot-related state accessible across the whole operating system.
*/

#include "hexaos.h"

HxRuntime Hx = {
  .rtos_ready = false,
  .safeboot = false,
  .config_loaded = false,
  .state_loaded = false,
  .littlefs_mounted = false,
  .uptime_ms = 0,
  .boot_count = 0
};