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

static constexpr const char* HX_CFG_DEVICE_NAME = "device.name";
static constexpr const char* HX_CFG_LOG_LEVEL = "log.level";
static constexpr const char* HX_CFG_SAFEBOOT_ENABLE = "safeboot.enable";

static constexpr const char* HX_STATE_BOOT_COUNT = "sys.boot_count";
static constexpr const char* HX_STATE_LAST_RESET = "sys.last_reset";

enum HxSchemaValueType : uint8_t {
  HX_SCHEMA_VALUE_BOOL = 0,
  HX_SCHEMA_VALUE_INT32 = 1,
  HX_SCHEMA_VALUE_STRING = 2,
  HX_SCHEMA_VALUE_LOG_LEVEL = 3
};

struct HxConfigKeyDef {
  const char* key;
  HxSchemaValueType type;
  size_t setup_offset;
  size_t value_size;
  int32_t min_i32;
  int32_t max_i32;
  size_t max_len;
  bool console_visible;
  bool console_writable;
};

struct HxStateKeyDef {
  const char* key;
  HxSchemaValueType type;
  int32_t min_i32;
  int32_t max_i32;
  size_t max_len;
  bool console_visible;
};

size_t SetupConfigKeyCount();
const HxConfigKeyDef* SetupConfigKeyAt(size_t index);
const HxConfigKeyDef* SetupFindConfigKey(const char* key);
bool SetupConfigValueToString(const HxConfigKeyDef* item, char* out, size_t out_size);
bool SetupConfigDefaultToString(const HxConfigKeyDef* item, char* out, size_t out_size);
bool SetupConfigSetValueFromString(const HxConfigKeyDef* item, const char* value);
bool SetupConfigResetValue(const HxConfigKeyDef* item);

size_t StateKeyCount();
const HxStateKeyDef* StateKeyAt(size_t index);
const HxStateKeyDef* StateFindKey(const char* key);
bool StateValueToString(const HxStateKeyDef* item, char* out, size_t out_size);