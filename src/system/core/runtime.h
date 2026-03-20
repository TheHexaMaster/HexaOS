/*
  HexaOS - runtime.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared runtime state declarations for HexaOS.
  Defines the global runtime structure that carries boot-time flags and system-wide state visible across the whole firmware.
*/

#pragma once

#include <stdint.h>

struct HxRuntime {
  bool rtos_ready;
  bool safeboot;
  bool config_loaded;
  bool state_loaded;
  bool littlefs_mounted;
  uint32_t uptime_ms;
  uint32_t boot_count;
};

extern HxRuntime Hx;
