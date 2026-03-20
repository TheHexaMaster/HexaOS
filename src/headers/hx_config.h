/*
  HexaOS - hx_config.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Defines the HexaOS runtime config structure stored in RAM together with
  the public lifecycle API used to initialize, load, save, reset and apply
  configuration. The schema is declared through typed entries so each value
  kind carries only the parameters that are meaningful for that type.
*/

#pragma once

// PRE-CONFIG Definitions and Macros
#include "include/pre_config.h"


// Typed config schema entries:
// XS(id, key_text, field_name, max_len, default_value, console_visible, console_writable)
// XI(id, key_text, field_name, min_i32, max_i32, default_value, console_visible, console_writable)
// XB(id, key_text, field_name, default_value, console_visible, console_writable)
// XF(id, key_text, field_name, min_f32, max_f32, default_value, console_visible, console_writable)
#define HX_CONFIG_SCHEMA(XS, XI, XB, XF) \
  XS(DEVICE_NAME,        "device.name",        device_name,       32,                         HX_BUILD_DEFAULT_DEVICE_NAME,                                         true, true) \
  XI(LOG_LEVEL,          "log.level",          log_level,         HX_LOG_ERROR,               HX_LOG_DEBUG,                 HX_BUILD_DEFAULT_LOG_LEVEL,             true, true) \
  XB(SAFEBOOT_ENABLE,    "safeboot.enable",    safeboot_enable,   HX_BUILD_DEFAULT_SAFEBOOT_ENABLE,                                                                 true, true) \
  XI(STATES_DELAY,       "states.delay",       states_delay,      0,                          600000,                      HX_CONFIG_DEFAULT_STATE_DELAY,           true, true)


// Typed state schema entries:
// XS(id, key_text, max_len, console_visible, write_restricted)
// XI(id, key_text, min_i32, max_i32, console_visible, write_restricted)
// XB(id, key_text, console_visible, write_restricted)
// XF(id, key_text, min_f32, max_f32, console_visible, write_restricted)
#define HX_STATE_SCHEMA(XS, XI, XB, XF) \
  XI(BOOT_COUNT,         "sys.boot_count",     0,          INT32_MAX, true, true) \
  XS(LAST_RESET,         "sys.last_reset",     32,         true,      true)


// POS-CONFIG Definitions and Macros
#include "include/pos_config.h"
