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


struct HxRuntime {
  bool safeboot;
  bool config_loaded;
  bool state_loaded;
  bool littlefs_mounted;
  uint32_t uptime_ms;
  uint32_t boot_count;
};

extern HxRuntime Hx;
