/*
  HexaOS - hx_config.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Defines the HexaOS runtime config structure stored in RAM together with
  the public lifecycle API used to initialize, load, save, reset and apply
  configuration. Per-key schema metadata and generic config item access are
  intentionally defined outside this header to keep the config contract small
  and stable.
*/

#pragma once

// PRE-CONFIG Definitions and Macros
#include "include/pre_config.h"



//   id                   key_text            type_id   field_name       storage size / max len / min I32 / max i32                      default value                            console - visible / writeable     
#define HX_CONFIG_SCHEMA(X) \
  X(DEVICE_NAME,        "device.name",        STRING,   device_name,      33,     32,     0,                         0,                         HX_BUILD_DEFAULT_DEVICE_NAME,               true,   true) \
  X(LOG_LEVEL,          "log.level",          INT32,    log_level,         0,     0,      (int32_t)HX_LOG_ERROR,    (int32_t)HX_LOG_DEBUG,     (int32_t)HX_BUILD_DEFAULT_LOG_LEVEL,         true,   true) \
  X(SAFEBOOT_ENABLE,    "safeboot.enable",    BOOL,     safeboot_enable,   0,     0,      0,                         1,                         (HX_BUILD_DEFAULT_SAFEBOOT_ENABLE != 0),    true,   true)


#define HX_STATE_SCHEMA(X) \
  X(BOOT_COUNT,         "sys.boot_count",     HX_SCHEMA_VALUE_INT32,  0, INT32_MAX, 0,  true) \
  X(LAST_RESET,         "sys.last_reset",     HX_SCHEMA_VALUE_STRING, 0, 0,         32, true)



// POS-CONFIG Definitions and Macros
#include "include/pos_config.h"

