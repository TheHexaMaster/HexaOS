/*
  HexaOS - hx_enums.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared fundamental HexaOS enums.
  Provides small cross-project enums and scalar type definitions that must stay lightweight and safe to include from any subsystem.
*/

#pragma once


// LOGS

enum HxLogLevel : uint8_t {
  HX_LOG_ERROR = 0,
  HX_LOG_WARN  = 1,
  HX_LOG_INFO  = 2,
  HX_LOG_DEBUG = 3
};


// RUNTIME DATA

struct HxRuntime {
  bool safeboot = false;
  bool config_loaded = false;
  bool state_loaded = false;
  bool littlefs_mounted = false;
  uint32_t uptime_ms = 0;
  uint32_t boot_count = 0;
};

inline HxRuntime Hx{};