/*
  HexaOS - hx_types.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared fundamental HexaOS types.
  Provides small cross-project enums and scalar type definitions that must stay lightweight and safe to include from any subsystem.
*/

#pragma once

#include <stdint.h>

enum HxLogLevel : uint8_t {
  HX_LOG_ERROR = 0,
  HX_LOG_WARN  = 1,
  HX_LOG_INFO  = 2,
  HX_LOG_DEBUG = 3
};