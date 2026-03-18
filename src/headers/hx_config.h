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

static constexpr size_t HX_CONFIG_DEVICE_NAME_MAX = 32;

struct HxConfig {
  char device_name[HX_CONFIG_DEVICE_NAME_MAX + 1];
  HxLogLevel log_level;
  bool safeboot_enable;
};

enum HxSchemaValueType : uint8_t {
  HX_SCHEMA_VALUE_BOOL = 0,
  HX_SCHEMA_VALUE_INT32 = 1,
  HX_SCHEMA_VALUE_STRING = 2,
  HX_SCHEMA_VALUE_LOG_LEVEL = 3
};

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

const char* ConfigLogLevelText(HxLogLevel level);
bool ConfigParseLogLevel(const char* text, HxLogLevel* level);