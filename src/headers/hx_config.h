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

#include "hx_build.h"

#include <stddef.h>
#include <stdint.h>


// Schema ENUM definition
enum HxSchemaValueType : uint8_t {
  HX_SCHEMA_VALUE_BOOL = 0,
  HX_SCHEMA_VALUE_INT32 = 1,
  HX_SCHEMA_VALUE_STRING = 2,
  HX_SCHEMA_VALUE_FLOAT = 3
};



// Typed config schema entries:
// XS(id, key_text, max_len, default_value, console_visible, console_writable)
// XI(id, key_text, min_i32, max_i32, default_value, console_visible, console_writable)
// XB(id, key_text, default_value, console_visible, console_writable)
// XF(id, key_text, min_f32, max_f32, default_value, console_visible, console_writable)
//
// The C token "id" is the single source for generated field names and compile-time symbols.
// Because the C preprocessor cannot derive valid identifiers from a string like "log.level",
// the schema keeps both a token id and an external text key.
#define HX_CONFIG_SCHEMA(XS, XI, XB, XF) \
  XS(user_string_a,      "user.string_a",       32,                                 "",                                       true, true) \
  XS(user_string_b,      "user.string_b",       32,                                 "",                                       true, true) \
  XS(user_string_c,      "user.string_c",       32,                                 "",                                       true, true) \
  XS(user_string_d,      "user.string_d",       32,                                 "",                                       true, true) \
  XS(user_string_e,      "user.string_e",       32,                                 "",                                       true, true) \
  XS(device_name,        "device.name",         32,                                 HX_CONFIG_DEFAULT_DEVICE_NAME,            true, true) \
  XI(log_level,          "log.level",           0,        4,                        HX_CONFIG_DEFAULT_LOG_LEVEL,              true, true) \
  XI(log_level_wc,       "log.level_wc",        0,        4,                        HX_CONFIG_DEFAULT_LOG_LEVEL,              true, true) \
  XB(safeboot_enable,    "safeboot.enable",                                         HX_CONFIG_DEFAULT_SAFEBOOT_ENABLE,        true, true) \
  XI(states_delay,       "states.delay",        0,        600000,                   HX_CONFIG_DEFAULT_STATE_DELAY,            true, true) \
  XS(board_pinmap,       "board.pinmap",        HX_BUILD_BOARD_PINMAP_MAX_LEN,      HX_BUILD_DEFAULT_BOARD_PINMAP_JSON,       true, false) \
  XS(drivers_bindings,   "drivers.bindings",    HX_BUILD_DRIVERS_BINDINGS_MAX_LEN,  HX_BUILD_DEFAULT_DRIVERS_BINDINGS_JSON,   true, false) \
  XS(wifi_ssid,          "wifi.ssid",           64,                                 "",                                       true,  true) \
  XS(wifi_password,      "wifi.password",       64,                                 "",                                       false, true)


// Typed state schema entries:
// XS(id, key_text, max_len, console_visible, write_restricted by user)
// XI(id, key_text, min_i32, max_i32, console_visible, write_restricted by user)
// XB(id, key_text, console_visible, write_restricted by user)
// XF(id, key_text, min_f32, max_f32, console_visible, write_restricted by user)
#define HX_STATE_SCHEMA(XS, XI, XB, XF) \
  XI(BOOT_COUNT,         "sys.boot_count",     0,          INT32_MAX,   true, true) \
  XS(LAST_RESET,         "sys.last_reset",     32,                      true, true)
