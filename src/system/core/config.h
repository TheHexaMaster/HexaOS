/*
  HexaOS - config.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core runtime configuration service interface.
  Declares the schema-driven configuration model, public config API and
  storage information contract used by the rest of HexaOS independently from
  the concrete persistence backend.
*/

#pragma once

#include "headers/hx_config.h"

#define HX_CFG_STRUCT_XS(id, key_text, max_len_value, default_value, console_visible_value, console_writable_value) \
  char id[(max_len_value) + 1];

#define HX_CFG_STRUCT_XI(id, key_text, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  int32_t id;

#define HX_CFG_STRUCT_XB(id, key_text, default_value, console_visible_value, console_writable_value) \
  bool id;

#define HX_CFG_STRUCT_XF(id, key_text, min_f32_value, max_f32_value, default_value, console_visible_value, console_writable_value) \
  float id;

struct HxConfig {
  HX_CONFIG_SCHEMA(HX_CFG_STRUCT_XS, HX_CFG_STRUCT_XI, HX_CFG_STRUCT_XB, HX_CFG_STRUCT_XF)
};

#undef HX_CFG_STRUCT_XF
#undef HX_CFG_STRUCT_XB
#undef HX_CFG_STRUCT_XI
#undef HX_CFG_STRUCT_XS

struct HxConfigKeyDef {
  const char* key;
  HxSchemaValueType type;
  size_t config_offset;
  size_t value_size;
  int32_t min_i32;
  int32_t max_i32;
  float min_f32;
  float max_f32;
  size_t max_len;
  bool console_visible;
  bool console_writable;
};

struct HxConfigStorageInfo {
  bool ready;
  bool loaded;
  const char* partition_label;
  const char* namespace_name;
  size_t entry_size_bytes;
  size_t total_key_count;
  size_t visible_key_count;
  size_t writable_key_count;
  size_t overridden_key_count;
  size_t partition_entries_used;
  size_t partition_entries_free;
  size_t partition_entries_available;
  size_t partition_entries_total;
  size_t namespace_entries_used;
};

size_t ConfigKeyCount();
const HxConfigKeyDef* ConfigKeyAt(size_t index);
const HxConfigKeyDef* ConfigFindConfigKey(const char* key);
bool ConfigValueToString(const HxConfigKeyDef* item, char* out, size_t out_size);
bool ConfigDefaultToString(const HxConfigKeyDef* item, char* out, size_t out_size);
bool ConfigSetValueFromString(const HxConfigKeyDef* item, const char* value);
bool ConfigResetValue(const HxConfigKeyDef* item);
bool ConfigToggleBool(const HxConfigKeyDef* item, bool* new_value_out);
bool ConfigGetStorageInfo(HxConfigStorageInfo* out_info);
bool ConfigFactoryFormat();



extern HxConfig HxConfigData;
extern const HxConfig HxConfigDefaults;

void ConfigResetToDefaults(HxConfig* config);

bool ConfigInit();
bool ConfigLoad();
bool ConfigSave();
void ConfigApply();



#define HX_CFG_KEY_DECLARE_XS(id, key_text, max_len_value, default_value, console_visible_value, console_writable_value) \
  static constexpr const char* HX_CFG_##id = key_text;

#define HX_CFG_KEY_DECLARE_XI(id, key_text, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  static constexpr const char* HX_CFG_##id = key_text;

#define HX_CFG_KEY_DECLARE_XB(id, key_text, default_value, console_visible_value, console_writable_value) \
  static constexpr const char* HX_CFG_##id = key_text;

#define HX_CFG_KEY_DECLARE_XF(id, key_text, min_f32_value, max_f32_value, default_value, console_visible_value, console_writable_value) \
  static constexpr const char* HX_CFG_##id = key_text;

HX_CONFIG_SCHEMA(HX_CFG_KEY_DECLARE_XS, HX_CFG_KEY_DECLARE_XI, HX_CFG_KEY_DECLARE_XB, HX_CFG_KEY_DECLARE_XF)

#undef HX_CFG_KEY_DECLARE_XF
#undef HX_CFG_KEY_DECLARE_XB
#undef HX_CFG_KEY_DECLARE_XI
#undef HX_CFG_KEY_DECLARE_XS