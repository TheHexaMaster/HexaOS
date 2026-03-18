/*
  HexaOS - hx_config_keys.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central persisted key schema for HexaOS.
  Defines the canonical NVS keys together with their value type,
  storage limits, console visibility and the metadata required by the
  generic config and state services.
*/

#pragma once


#define HX_CONFIG_SCHEMA(X) \
  X(DEVICE_NAME,        "device.name",        HX_SCHEMA_VALUE_STRING,       device_name,      HX_CONFIG_DEVICE_NAME_MAX, 0,                  0,                  true, true) \
  X(LOG_LEVEL,          "log.level",          HX_SCHEMA_VALUE_LOG_LEVEL,    log_level,        0,                         (int32_t)HX_LOG_ERROR, (int32_t)HX_LOG_DEBUG, true, true) \
  X(SAFEBOOT_ENABLE,    "safeboot.enable",    HX_SCHEMA_VALUE_BOOL,         safeboot_enable,  0,                         0,                  1,                  true, true)

#define HX_STATE_SCHEMA(X) \
  X(BOOT_COUNT,         "sys.boot_count",     HX_SCHEMA_VALUE_INT32,  0, INT32_MAX, 0,  true) \
  X(LAST_RESET,         "sys.last_reset",     HX_SCHEMA_VALUE_STRING, 0, 0,         32, true)

#define HX_CFG_KEY_DECLARE(id, key_text, type_id, field, max_len, min_i32, max_i32, console_visible, console_writable) \
  static constexpr const char* HX_CFG_##id = key_text;

HX_CONFIG_SCHEMA(HX_CFG_KEY_DECLARE)

#undef HX_CFG_KEY_DECLARE

#define HX_STATE_KEY_DECLARE(id, key_text, type_id, min_i32, max_i32, max_len, console_visible) \
  static constexpr const char* HX_STATE_##id = key_text;

HX_STATE_SCHEMA(HX_STATE_KEY_DECLARE)

#undef HX_STATE_KEY_DECLARE  