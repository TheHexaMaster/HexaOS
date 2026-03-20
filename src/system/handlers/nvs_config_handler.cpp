/*
  HexaOS - nvs_config_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Runtime configuration service.
  Provides the persistent key-value configuration layer stored in NVS and used to load, query and save mutable HexaOS config values across reboots.
*/

#include "hexaos.h"
#include "system/adapters/nvs_adapter.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool g_config_ready = false;

HxConfig HxConfigData = {};
const HxConfig HxConfigDefaults = {
#define HX_CONFIG_DEFAULT_ITEM(id, key_text, type_id, field_name, storage_size, max_len_value, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  default_value,

  HX_CONFIG_SCHEMA(HX_CONFIG_DEFAULT_ITEM)

#undef HX_CONFIG_DEFAULT_ITEM
};

static const HxConfigKeyDef kHxConfigKeys[] = {
#define HX_CONFIG_ITEM(id, key_text, type_id, field_name, storage_size, max_len_value, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  { \
    .key = key_text, \
    .type = type_id, \
    .config_offset = offsetof(HxConfig, field_name), \
    .value_size = sizeof(((HxConfig*)0)->field_name), \
    .min_i32 = (int32_t)(min_i32_value), \
    .max_i32 = (int32_t)(max_i32_value), \
    .max_len = (size_t)(max_len_value), \
    .console_visible = (console_visible_value), \
    .console_writable = (console_writable_value) \
  }, 

  HX_CONFIG_SCHEMA(HX_CONFIG_ITEM)

#undef HX_CONFIG_ITEM
};


static void* ConfigFieldPtr(HxConfig* config, const HxConfigKeyDef* item) {
  if (!config || !item) {
    return nullptr;
  }

  return (void*)(reinterpret_cast<uint8_t*>(config) + item->config_offset);
}

static const void* ConfigFieldPtrConst(const HxConfig* config, const HxConfigKeyDef* item) {
  if (!config || !item) {
    return nullptr;
  }

  return (const void*)(reinterpret_cast<const uint8_t*>(config) + item->config_offset);
}

static bool ConfigParseBoolText(const char* text, bool* value) {
  if (!text || !text[0] || !value) {
    return false;
  }

  if ((strcasecmp(text, "1") == 0) ||
      (strcasecmp(text, "on") == 0) ||
      (strcasecmp(text, "true") == 0) ||
      (strcasecmp(text, "yes") == 0)) {
    *value = true;
    return true;
  }

  if ((strcasecmp(text, "0") == 0) ||
      (strcasecmp(text, "off") == 0) ||
      (strcasecmp(text, "false") == 0) ||
      (strcasecmp(text, "no") == 0)) {
    *value = false;
    return true;
  }

  return false;
}

static bool ConfigParseInt32Text(const char* text, int32_t min_value, int32_t max_value, int32_t* value) {
  if (!text || !text[0] || !value) {
    return false;
  }

  char* endptr = nullptr;
  long raw = strtol(text, &endptr, 10);
  if (!endptr || (*endptr != '\0')) {
    return false;
  }

  if ((raw < (long)min_value) || (raw > (long)max_value)) {
    return false;
  }

  *value = (int32_t)raw;
  return true;
}

static bool ConfigAssignStringField(HxConfig* config, const HxConfigKeyDef* item, const char* value) {
  if (!config || !item || !value) {
    return false;
  }

  if ((item->type != HX_SCHEMA_VALUE_STRING) || (item->value_size == 0) || (item->max_len == 0)) {
    return false;
  }

  const size_t len = strlen(value);
  if ((len == 0) || (len > item->max_len) || (item->value_size < (len + 1))) {
    return false;
  }

  char* field = static_cast<char*>(ConfigFieldPtr(config, item));
  if (!field) {
    return false;
  }

  memset(field, 0, item->value_size);
  memcpy(field, value, len);
  field[len] = '\0';
  return true;
}

static bool ConfigAssignBoolField(HxConfig* config, const HxConfigKeyDef* item, bool value) {
  if (!config || !item || (item->type != HX_SCHEMA_VALUE_BOOL) || (item->value_size != sizeof(bool))) {
    return false;
  }

  bool* field = static_cast<bool*>(ConfigFieldPtr(config, item));
  if (!field) {
    return false;
  }

  *field = value;
  return true;
}

static bool ConfigAssignInt32Field(HxConfig* config, const HxConfigKeyDef* item, int32_t value) {
  if (!config || !item) {
    return false;
  }

  if (item->type != HX_SCHEMA_VALUE_INT32) {
    return false;
  }

  if ((value < item->min_i32) || (value > item->max_i32)) {
    return false;
  }

  if (item->value_size != sizeof(int32_t)) {
    return false;
  }

  int32_t* field = static_cast<int32_t*>(ConfigFieldPtr(config, item));
  if (!field) {
    return false;
  }

  *field = value;
  return true;
}

static bool ConfigAssignValueFromString(HxConfig* config, const HxConfigKeyDef* item, const char* value) {
  if (!config || !item || !value) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING:
      return ConfigAssignStringField(config, item, value);

    case HX_SCHEMA_VALUE_BOOL: {
      bool parsed = false;
      if (!ConfigParseBoolText(value, &parsed)) {
        return false;
      }
      return ConfigAssignBoolField(config, item, parsed);
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t parsed = 0;

        if (!ConfigParseInt32Text(value, item->min_i32, item->max_i32, &parsed)) {
          return false;
        }

      return ConfigAssignInt32Field(config, item, parsed);
    }

    default:
      return false;
  }
}

static bool ConfigReadItemFromNvs(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING: {
      String text_value;
      if (!HxNvsGetString(HX_NVS_STORE_CONFIG, item->key, text_value)) {
        return true;
      }
      return ConfigAssignStringField(&HxConfigData, item, text_value.c_str());
    }

    case HX_SCHEMA_VALUE_BOOL: {
      bool bool_value = false;
      if (!HxNvsGetBool(HX_NVS_STORE_CONFIG, item->key, &bool_value)) {
        return true;
      }
      return ConfigAssignBoolField(&HxConfigData, item, bool_value);
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t int_value = 0;
      if (!HxNvsGetInt(HX_NVS_STORE_CONFIG, item->key, &int_value)) {
        return true;
      }
      return ConfigAssignInt32Field(&HxConfigData, item, int_value);
    }

    default:
      return false;
  }
}

static bool ConfigItemEqualsDefault(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  const void* current_ptr = ConfigFieldPtrConst(&HxConfigData, item);
  const void* default_ptr = ConfigFieldPtrConst(&HxConfigDefaults, item);
  if (!current_ptr || !default_ptr) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING:
      return strcmp(static_cast<const char*>(current_ptr), static_cast<const char*>(default_ptr)) == 0;

    case HX_SCHEMA_VALUE_BOOL:
      return (*static_cast<const bool*>(current_ptr) == *static_cast<const bool*>(default_ptr));

    case HX_SCHEMA_VALUE_INT32:
      return (*static_cast<const int32_t*>(current_ptr) == *static_cast<const int32_t*>(default_ptr));

    default:
      return false;
  }
}

static bool ConfigStoreItemOverride(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  if (ConfigItemEqualsDefault(item)) {
    return HxNvsEraseKey(HX_NVS_STORE_CONFIG, item->key);
  }

  const void* current_ptr = ConfigFieldPtrConst(&HxConfigData, item);
  if (!current_ptr) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING:
      return HxNvsSetString(HX_NVS_STORE_CONFIG, item->key, static_cast<const char*>(current_ptr));

    case HX_SCHEMA_VALUE_BOOL:
      return HxNvsSetBool(HX_NVS_STORE_CONFIG, item->key, *static_cast<const bool*>(current_ptr));

    case HX_SCHEMA_VALUE_INT32:
      return HxNvsSetInt(HX_NVS_STORE_CONFIG, item->key, *static_cast<const int32_t*>(current_ptr));

    default:
      return false;
  }
}

size_t ConfigKeyCount() {
  return sizeof(kHxConfigKeys) / sizeof(kHxConfigKeys[0]);
}

const HxConfigKeyDef* ConfigKeyAt(size_t index) {
  if (index >= ConfigKeyCount()) {
    return nullptr;
  }

  return &kHxConfigKeys[index];
}

const HxConfigKeyDef* ConfigFindConfigKey(const char* key) {
  if (!key || !key[0]) {
    return nullptr;
  }

  for (size_t i = 0; i < ConfigKeyCount(); i++) {
    if (strcmp(kHxConfigKeys[i].key, key) == 0) {
      return &kHxConfigKeys[i];
    }
  }

  return nullptr;
}

void ConfigResetToDefaults(HxConfig* config) {
  if (!config) {
    return;
  }

  *config = HxConfigDefaults;
}

bool ConfigValueToString(const HxConfigKeyDef* item, char* out, size_t out_size) {
  if (!item || !out || (out_size == 0)) {
    return false;
  }

  out[0] = '\0';

  const void* ptr = ConfigFieldPtrConst(&HxConfigData, item);
  if (!ptr) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING:
      snprintf(out, out_size, "%s", static_cast<const char*>(ptr));
      return true;

    case HX_SCHEMA_VALUE_BOOL:
      snprintf(out, out_size, "%s", *static_cast<const bool*>(ptr) ? "true" : "false");
      return true;

    case HX_SCHEMA_VALUE_INT32:
      snprintf(out, out_size, "%ld", (long)(*static_cast<const int32_t*>(ptr)));
      return true;

    default:
      return false;
  }
}

bool ConfigDefaultToString(const HxConfigKeyDef* item, char* out, size_t out_size) {
  if (!item || !out || (out_size == 0)) {
    return false;
  }

  out[0] = '\0';

  const void* ptr = ConfigFieldPtrConst(&HxConfigDefaults, item);
  if (!ptr) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING:
      snprintf(out, out_size, "%s", static_cast<const char*>(ptr));
      return true;

    case HX_SCHEMA_VALUE_BOOL:
      snprintf(out, out_size, "%s", *static_cast<const bool*>(ptr) ? "true" : "false");
      return true;

    case HX_SCHEMA_VALUE_INT32:
      snprintf(out, out_size, "%ld", (long)(*static_cast<const int32_t*>(ptr)));
      return true;

    default:
      return false;
  }
}

bool ConfigSetValueFromString(const HxConfigKeyDef* item, const char* value) {
  if (!item || !value || !item->console_writable) {
    return false;
  }

  return ConfigAssignValueFromString(&HxConfigData, item, value);
}

bool ConfigResetValue(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  const void* default_ptr = ConfigFieldPtrConst(&HxConfigDefaults, item);
  void* current_ptr = ConfigFieldPtr(&HxConfigData, item);
  if (!default_ptr || !current_ptr || (item->value_size == 0)) {
    return false;
  }

  memcpy(current_ptr, default_ptr, item->value_size);
  return true;
}

bool ConfigInit() {
  ConfigResetToDefaults(&HxConfigData);
  g_config_ready = EspNvsOpenConfig();
  return g_config_ready;
}

bool ConfigLoad() {
  ConfigResetToDefaults(&HxConfigData);

  if (!g_config_ready) {
    Hx.config_loaded = false;
    return false;
  }

  for (size_t i = 0; i < ConfigKeyCount(); i++) {
    if (!ConfigReadItemFromNvs(&kHxConfigKeys[i])) {
      Hx.config_loaded = false;
      return false;
    }
  }

  Hx.config_loaded = true;
  return true;
}

bool ConfigSave() {
  if (!g_config_ready) {
    return false;
  }

  for (size_t i = 0; i < ConfigKeyCount(); i++) {
    if (!ConfigStoreItemOverride(&kHxConfigKeys[i])) {
      return false;
    }
  }

  if (!HxNvsCommit(HX_NVS_STORE_CONFIG)) {
    return false;
  }

  Hx.config_loaded = true;
  return true;
}

void ConfigApply() {
  LogSetLevel((HxLogLevel)HxConfigData.log_level);
  Hx.safeboot = HxConfigData.safeboot_enable;
}