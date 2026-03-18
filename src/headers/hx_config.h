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

enum HxSchemaValueType : uint8_t {
  HX_SCHEMA_VALUE_BOOL = 0,
  HX_SCHEMA_VALUE_INT32 = 1,
  HX_SCHEMA_VALUE_STRING = 2
};

#define HX_CONFIG_SCHEMA(X) \
  X(DEVICE_NAME,        "device.name",        STRING,       device_name,      33, 32, 0,                         0,                         HX_BUILD_DEFAULT_DEVICE_NAME,               true, true) \
  X(LOG_LEVEL,          "log.level",          INT32,        log_level,         0,  0, (int32_t)HX_LOG_ERROR,    (int32_t)HX_LOG_DEBUG,     (int32_t)HX_BUILD_DEFAULT_LOG_LEVEL,        true, true) \
  X(SAFEBOOT_ENABLE,    "safeboot.enable",    BOOL,         safeboot_enable,   0,  0, 0,                         1,                         (HX_BUILD_DEFAULT_SAFEBOOT_ENABLE != 0),    true, true)


#define HX_STATE_SCHEMA(X) \
  X(BOOT_COUNT,         "sys.boot_count",     HX_SCHEMA_VALUE_INT32,  0, INT32_MAX, 0,  true) \
  X(LAST_RESET,         "sys.last_reset",     HX_SCHEMA_VALUE_STRING, 0, 0,         32, true)

#define HX_CFG_FIELD_STRING(field_name, storage_size) char field_name[storage_size];
#define HX_CFG_FIELD_BOOL(field_name, storage_size) bool field_name;
#define HX_CFG_FIELD_INT32(field_name, storage_size) int32_t field_name;

#define HX_CFG_FIELD_SELECT_(type_id, field_name, storage_size) HX_CFG_FIELD_##type_id(field_name, storage_size)
#define HX_CFG_FIELD_SELECT(type_id, field_name, storage_size) HX_CFG_FIELD_SELECT_(type_id, field_name, storage_size)

struct HxConfig {
#define HX_CFG_STRUCT_FIELD(id, key_text, type_id, field_name, storage_size, max_len_value, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  HX_CFG_FIELD_SELECT(type_id, field_name, storage_size)

  HX_CONFIG_SCHEMA(HX_CFG_STRUCT_FIELD)

#undef HX_CFG_STRUCT_FIELD
};

#undef HX_CFG_FIELD_SELECT
#undef HX_CFG_FIELD_SELECT_
#undef HX_CFG_FIELD_INT32
#undef HX_CFG_FIELD_BOOL
#undef HX_CFG_FIELD_STRING

struct HxConfigKeyDef {
  const char* key;
  HxSchemaValueType type;
  size_t config_offset;
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

size_t ConfigConfigKeyCount();
const HxConfigKeyDef* ConfigConfigKeyAt(size_t index);
const HxConfigKeyDef* ConfigFindConfigKey(const char* key);
bool ConfigConfigValueToString(const HxConfigKeyDef* item, char* out, size_t out_size);
bool ConfigConfigDefaultToString(const HxConfigKeyDef* item, char* out, size_t out_size);
bool ConfigConfigSetValueFromString(const HxConfigKeyDef* item, const char* value);
bool ConfigConfigResetValue(const HxConfigKeyDef* item);

size_t StateKeyCount();
const HxStateKeyDef* StateKeyAt(size_t index);
const HxStateKeyDef* StateFindKey(const char* key);
bool StateValueToString(const HxStateKeyDef* item, char* out, size_t out_size);

extern HxConfig HxConfigData;
extern const HxConfig HxConfigDefaults;

void ConfigResetToDefaults(HxConfig* config);

bool ConfigInit();
bool ConfigLoad();
bool ConfigSave();
void ConfigApply();


#define HX_CFG_KEY_DECLARE(id, key_text, type_id, field_name, storage_size, max_len_value, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  static constexpr const char* HX_CFG_##id = key_text;

HX_CONFIG_SCHEMA(HX_CFG_KEY_DECLARE)

#undef HX_CFG_KEY_DECLARE

#define HX_STATE_KEY_DECLARE(id, key_text, type_id, min_i32, max_i32, max_len, console_visible) \
  static constexpr const char* HX_STATE_##id = key_text;

HX_STATE_SCHEMA(HX_STATE_KEY_DECLARE)

#undef HX_STATE_KEY_DECLARE