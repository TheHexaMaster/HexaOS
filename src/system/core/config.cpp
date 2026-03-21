/*
  HexaOS - config.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core runtime configuration service.
  Owns HexaOS configuration policy, schema validation, defaults and storage
  routing while delegating concrete persistence operations to the unified NVS
  adapter.
*/


#include "config.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "system/adapters/nvs_adapter.h"
#include "system/core/log.h"
#include "system/core/rtos.h"
#include "system/core/runtime.h"


static bool g_config_ready = false;
static HxRtosCritical g_config_critical = HX_RTOS_CRITICAL_INIT;
static constexpr size_t HX_CONFIG_NVS_ENTRY_SIZE_BYTES = 32;


static void ConfigStateEnter() {
  RtosCriticalEnter(&g_config_critical);
}

static void ConfigStateExit() {
  RtosCriticalExit(&g_config_critical);
}

enum ConfigLoadItemResult : uint8_t {
  HX_CONFIG_LOAD_ITEM_OK = 0,
  HX_CONFIG_LOAD_ITEM_MISSING = 1,
  HX_CONFIG_LOAD_ITEM_INVALID = 2,
  HX_CONFIG_LOAD_ITEM_ERROR = 3
};

static void ConfigFormatFloatDisplay(char* out, size_t out_size, float value) {
  if (!out || (out_size == 0)) {
    return;
  }

  if (!isfinite(value)) {
    if (isnan(value)) {
      snprintf(out, out_size, "nan");
    } else {
      snprintf(out, out_size, "%s", (value < 0.0f) ? "-inf" : "inf");
    }
    return;
  }

  snprintf(out, out_size, "%.7g", (double)value);
  if (strcmp(out, "-0") == 0) {
    snprintf(out, out_size, "0");
  }
}

HxConfig HxConfigData = {};
const HxConfig HxConfigDefaults = {
#define HX_CONFIG_DEFAULT_XS(id, key_text, max_len_value, default_value, console_visible_value, console_writable_value) \
  default_value,
#define HX_CONFIG_DEFAULT_XI(id, key_text, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  default_value,
#define HX_CONFIG_DEFAULT_XB(id, key_text, default_value, console_visible_value, console_writable_value) \
  default_value,
#define HX_CONFIG_DEFAULT_XF(id, key_text, min_f32_value, max_f32_value, default_value, console_visible_value, console_writable_value) \
  default_value,

  HX_CONFIG_SCHEMA(HX_CONFIG_DEFAULT_XS, HX_CONFIG_DEFAULT_XI, HX_CONFIG_DEFAULT_XB, HX_CONFIG_DEFAULT_XF)

#undef HX_CONFIG_DEFAULT_XF
#undef HX_CONFIG_DEFAULT_XB
#undef HX_CONFIG_DEFAULT_XI
#undef HX_CONFIG_DEFAULT_XS
};

static const HxConfigKeyDef kHxConfigKeys[] = {
#define HX_CONFIG_ITEM_XS(id, key_text, max_len_value, default_value, console_visible_value, console_writable_value) \
  { \
    .key = key_text, \
    .type = HX_SCHEMA_VALUE_STRING, \
    .config_offset = offsetof(HxConfig, id), \
    .value_size = sizeof(((HxConfig*)0)->id), \
    .min_i32 = 0, \
    .max_i32 = 0, \
    .min_f32 = 0.0f, \
    .max_f32 = 0.0f, \
    .max_len = (size_t)(max_len_value), \
    .console_visible = (console_visible_value), \
    .console_writable = (console_writable_value) \
  },
#define HX_CONFIG_ITEM_XI(id, key_text, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  { \
    .key = key_text, \
    .type = HX_SCHEMA_VALUE_INT32, \
    .config_offset = offsetof(HxConfig, id), \
    .value_size = sizeof(((HxConfig*)0)->id), \
    .min_i32 = (int32_t)(min_i32_value), \
    .max_i32 = (int32_t)(max_i32_value), \
    .min_f32 = 0.0f, \
    .max_f32 = 0.0f, \
    .max_len = 0, \
    .console_visible = (console_visible_value), \
    .console_writable = (console_writable_value) \
  },
#define HX_CONFIG_ITEM_XB(id, key_text, default_value, console_visible_value, console_writable_value) \
  { \
    .key = key_text, \
    .type = HX_SCHEMA_VALUE_BOOL, \
    .config_offset = offsetof(HxConfig, id), \
    .value_size = sizeof(((HxConfig*)0)->id), \
    .min_i32 = 0, \
    .max_i32 = 0, \
    .min_f32 = 0.0f, \
    .max_f32 = 0.0f, \
    .max_len = 0, \
    .console_visible = (console_visible_value), \
    .console_writable = (console_writable_value) \
  },
#define HX_CONFIG_ITEM_XF(id, key_text, min_f32_value, max_f32_value, default_value, console_visible_value, console_writable_value) \
  { \
    .key = key_text, \
    .type = HX_SCHEMA_VALUE_FLOAT, \
    .config_offset = offsetof(HxConfig, id), \
    .value_size = sizeof(((HxConfig*)0)->id), \
    .min_i32 = 0, \
    .max_i32 = 0, \
    .min_f32 = (float)(min_f32_value), \
    .max_f32 = (float)(max_f32_value), \
    .max_len = 0, \
    .console_visible = (console_visible_value), \
    .console_writable = (console_writable_value) \
  },

  HX_CONFIG_SCHEMA(HX_CONFIG_ITEM_XS, HX_CONFIG_ITEM_XI, HX_CONFIG_ITEM_XB, HX_CONFIG_ITEM_XF)

#undef HX_CONFIG_ITEM_XF
#undef HX_CONFIG_ITEM_XB
#undef HX_CONFIG_ITEM_XI
#undef HX_CONFIG_ITEM_XS
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

static void ConfigCopyCurrent(HxConfig* out_config) {
  if (!out_config) {
    return;
  }

  ConfigStateEnter();
  *out_config = HxConfigData;
  ConfigStateExit();
}

static void ConfigCommitCurrent(const HxConfig* config, bool loaded) {
  if (!config) {
    return;
  }

  ConfigStateEnter();
  HxConfigData = *config;
  Hx.config_loaded = loaded;
  ConfigStateExit();
}

static bool ConfigLoadedFlag() {
  bool loaded = false;
  ConfigStateEnter();
  loaded = Hx.config_loaded;
  ConfigStateExit();
  return loaded;
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

  errno = 0;
  char* endptr = nullptr;
  long raw = strtol(text, &endptr, 10);
  if ((errno != 0) || (endptr == text)) {
    return false;
  }

  while ((*endptr == ' ') || (*endptr == '\t')) {
    endptr++;
  }

  if (*endptr != '\0') {
    return false;
  }

  if ((raw < (long)min_value) || (raw > (long)max_value)) {
    return false;
  }

  *value = (int32_t)raw;
  return true;
}

static bool ConfigParseFloatText(const char* text, float min_value, float max_value, float* value) {
  if (!text || !text[0] || !value) {
    return false;
  }

  errno = 0;
  char* endptr = nullptr;
  float raw = strtof(text, &endptr);
  if ((errno != 0) || (endptr == text)) {
    return false;
  }

  while ((*endptr == ' ') || (*endptr == '\t')) {
    endptr++;
  }

  if (*endptr != '\0') {
    return false;
  }

  if (!isfinite(raw) || (raw < min_value) || (raw > max_value)) {
    return false;
  }

  *value = raw;
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
  if ((len > item->max_len) || (item->value_size < (len + 1))) {
    return false;
  }

  char* field = static_cast<char*>(ConfigFieldPtr(config, item));
  if (!field) {
    return false;
  }

  memset(field, 0, item->value_size);
  if (len > 0) {
    memcpy(field, value, len);
  }
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

static bool ConfigAssignFloatField(HxConfig* config, const HxConfigKeyDef* item, float value) {
  if (!config || !item) {
    return false;
  }

  if (item->type != HX_SCHEMA_VALUE_FLOAT) {
    return false;
  }

  if (!isfinite(value) || (value < item->min_f32) || (value > item->max_f32)) {
    return false;
  }

  if (item->value_size != sizeof(float)) {
    return false;
  }

  float* field = static_cast<float*>(ConfigFieldPtr(config, item));
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

    case HX_SCHEMA_VALUE_FLOAT: {
      float parsed = 0.0f;
      if (!ConfigParseFloatText(value, item->min_f32, item->max_f32, &parsed)) {
        return false;
      }
      return ConfigAssignFloatField(config, item, parsed);
    }

    default:
      return false;
  }
}

static bool ConfigItemEqualsDefaultInConfig(const HxConfig* config, const HxConfigKeyDef* item) {
  if (!config || !item) {
    return false;
  }

  const void* current_ptr = ConfigFieldPtrConst(config, item);
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

    case HX_SCHEMA_VALUE_FLOAT:
      return (*static_cast<const float*>(current_ptr) == *static_cast<const float*>(default_ptr));

    default:
      return false;
  }
}

static bool ConfigStoreItemOverrideFromConfig(const HxConfig* config, const HxConfigKeyDef* item) {
  if (!config || !item) {
    return false;
  }

  if (ConfigItemEqualsDefaultInConfig(config, item)) {
    return HxNvsEraseKey(HX_NVS_STORE_CONFIG, item->key);
  }

  const void* current_ptr = ConfigFieldPtrConst(config, item);
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

    case HX_SCHEMA_VALUE_FLOAT:
      return HxNvsSetFloat(HX_NVS_STORE_CONFIG, item->key, *static_cast<const float*>(current_ptr));

    default:
      return false;
  }
}

static ConfigLoadItemResult ConfigReadItemFromNvs(HxConfig* config, const HxConfigKeyDef* item) {
  if (!config || !item) {
    return HX_CONFIG_LOAD_ITEM_ERROR;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING: {
      String text_value;
      HxNvsReadResult result = HxNvsReadString(HX_NVS_STORE_CONFIG, item->key, text_value);
      if (result == HX_NVS_READ_NOT_FOUND) {
        return HX_CONFIG_LOAD_ITEM_MISSING;
      }
      if (result != HX_NVS_READ_OK) {
        return HX_CONFIG_LOAD_ITEM_ERROR;
      }
      return ConfigAssignStringField(config, item, text_value.c_str()) ? HX_CONFIG_LOAD_ITEM_OK : HX_CONFIG_LOAD_ITEM_INVALID;
    }

    case HX_SCHEMA_VALUE_BOOL: {
      bool bool_value = false;
      HxNvsReadResult result = HxNvsReadBool(HX_NVS_STORE_CONFIG, item->key, &bool_value);
      if (result == HX_NVS_READ_NOT_FOUND) {
        return HX_CONFIG_LOAD_ITEM_MISSING;
      }
      if (result != HX_NVS_READ_OK) {
        return HX_CONFIG_LOAD_ITEM_ERROR;
      }
      return ConfigAssignBoolField(config, item, bool_value) ? HX_CONFIG_LOAD_ITEM_OK : HX_CONFIG_LOAD_ITEM_INVALID;
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t int_value = 0;
      HxNvsReadResult result = HxNvsReadInt(HX_NVS_STORE_CONFIG, item->key, &int_value);
      if (result == HX_NVS_READ_NOT_FOUND) {
        return HX_CONFIG_LOAD_ITEM_MISSING;
      }
      if (result != HX_NVS_READ_OK) {
        return HX_CONFIG_LOAD_ITEM_ERROR;
      }
      return ConfigAssignInt32Field(config, item, int_value) ? HX_CONFIG_LOAD_ITEM_OK : HX_CONFIG_LOAD_ITEM_INVALID;
    }

    case HX_SCHEMA_VALUE_FLOAT: {
      float float_value = 0.0f;
      HxNvsReadResult result = HxNvsReadFloat(HX_NVS_STORE_CONFIG, item->key, &float_value);
      if (result == HX_NVS_READ_NOT_FOUND) {
        return HX_CONFIG_LOAD_ITEM_MISSING;
      }
      if (result != HX_NVS_READ_OK) {
        return HX_CONFIG_LOAD_ITEM_ERROR;
      }
      return ConfigAssignFloatField(config, item, float_value) ? HX_CONFIG_LOAD_ITEM_OK : HX_CONFIG_LOAD_ITEM_INVALID;
    }

    default:
      return HX_CONFIG_LOAD_ITEM_ERROR;
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

  if (config == &HxConfigData) {
    ConfigStateEnter();
    HxConfigData = HxConfigDefaults;
    ConfigStateExit();
    return;
  }

  *config = HxConfigDefaults;
}

bool ConfigValueToString(const HxConfigKeyDef* item, char* out, size_t out_size) {
  if (!item || !out || (out_size == 0)) {
    return false;
  }

  out[0] = '\0';

  HxConfig snapshot{};
  ConfigCopyCurrent(&snapshot);

  const void* ptr = ConfigFieldPtrConst(&snapshot, item);
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

    case HX_SCHEMA_VALUE_FLOAT:
      ConfigFormatFloatDisplay(out, out_size, *static_cast<const float*>(ptr));
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

    case HX_SCHEMA_VALUE_FLOAT:
      ConfigFormatFloatDisplay(out, out_size, *static_cast<const float*>(ptr));
      return true;

    default:
      return false;
  }
}

bool ConfigSetValueFromString(const HxConfigKeyDef* item, const char* value) {
  if (!item || !value || !item->console_writable) {
    return false;
  }

  HxConfig next = {};
  ConfigCopyCurrent(&next);
  if (!ConfigAssignValueFromString(&next, item, value)) {
    return false;
  }

  ConfigCommitCurrent(&next, ConfigLoadedFlag());
  HX_LOGD("CFG", "set key=%s", item->key);
  return true;
}

bool ConfigResetValue(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  const void* default_ptr = ConfigFieldPtrConst(&HxConfigDefaults, item);
  if (!default_ptr || (item->value_size == 0)) {
    return false;
  }

  HxConfig next = {};
  ConfigCopyCurrent(&next);

  void* current_ptr = ConfigFieldPtr(&next, item);
  if (!current_ptr) {
    return false;
  }

  memcpy(current_ptr, default_ptr, item->value_size);
  ConfigCommitCurrent(&next, ConfigLoadedFlag());
  HX_LOGD("CFG", "reset key=%s to default", item->key);
  return true;
}

bool ConfigToggleBool(const HxConfigKeyDef* item, bool* new_value_out) {
  if (!item || (item->type != HX_SCHEMA_VALUE_BOOL) || !item->console_writable || (item->value_size != sizeof(bool))) {
    return false;
  }

  HxConfig next = {};
  ConfigCopyCurrent(&next);

  bool* field = static_cast<bool*>(ConfigFieldPtr(&next, item));
  if (!field) {
    return false;
  }

  *field = !(*field);
  if (new_value_out) {
    *new_value_out = *field;
  }

  ConfigCommitCurrent(&next, ConfigLoadedFlag());
  HX_LOGD("CFG", "toggle key=%s value=%s", item->key, *field ? "true" : "false");
  return true;
}

bool ConfigInit() {
  if (!RtosCriticalReady(&g_config_critical) && !RtosCriticalInit(&g_config_critical)) {
    HX_LOGE("CFG", "critical init failed");
    return false;
  }

  ConfigResetToDefaults(&HxConfigData);
  ConfigStateEnter();
  Hx.config_loaded = false;
  ConfigStateExit();

  g_config_ready = HxNvsOpen(HX_NVS_STORE_CONFIG);
  if (g_config_ready) {
    HX_LOGI("CFG", "config store ready (%s)", "nvs");
  } else {
    HX_LOGE("CFG", "config store open failed");
  }

  return g_config_ready;
}

bool ConfigLoad() {
  HxConfig loaded = {};
  ConfigResetToDefaults(&loaded);

  if (!g_config_ready) {
    ConfigCommitCurrent(&loaded, false);
    HX_LOGE("CFG", "load failed, config store not ready");
    return false;
  }

  size_t loaded_count = 0;
  size_t missing_count = 0;
  size_t invalid_count = 0;
  size_t error_count = 0;
  bool repair_dirty = false;

  for (size_t i = 0; i < ConfigKeyCount(); i++) {
    const HxConfigKeyDef* item = &kHxConfigKeys[i];
    ConfigLoadItemResult result = ConfigReadItemFromNvs(&loaded, item);

    switch (result) {
      case HX_CONFIG_LOAD_ITEM_OK:
        loaded_count++;
        HX_LOGD("CFG", "override loaded key=%s", item->key);
        break;

      case HX_CONFIG_LOAD_ITEM_MISSING:
        missing_count++;
        break;

      case HX_CONFIG_LOAD_ITEM_INVALID:
        invalid_count++;
        HX_LOGW("CFG", "invalid override ignored, default used: key=%s", item->key);
        if (HxNvsEraseKey(HX_NVS_STORE_CONFIG, item->key)) {
          repair_dirty = true;
        } else {
          HX_LOGW("CFG", "failed to stage invalid override cleanup: key=%s", item->key);
        }
        break;

      case HX_CONFIG_LOAD_ITEM_ERROR:
      default:
        error_count++;
        HX_LOGW("CFG", "read error ignored, default used: key=%s", item->key);
        break;
    }
  }

  if (repair_dirty) {
    if (HxNvsCommit(HX_NVS_STORE_CONFIG)) {
      HX_LOGW("CFG", "invalid overrides removed during load");
    } else {
      HX_LOGW("CFG", "invalid override cleanup commit failed");
    }
  }

  ConfigCommitCurrent(&loaded, true);

  HX_LOGI("CFG", "load complete overrides=%lu missing=%lu invalid=%lu errors=%lu",
          (unsigned long)loaded_count,
          (unsigned long)missing_count,
          (unsigned long)invalid_count,
          (unsigned long)error_count);
  return true;
}

bool ConfigSave() {
  if (!g_config_ready) {
    HX_LOGE("CFG", "save failed, config store not ready");
    return false;
  }

  HxConfig snapshot = {};
  ConfigCopyCurrent(&snapshot);

  size_t overridden_count = 0;
  size_t default_count = 0;

  for (size_t i = 0; i < ConfigKeyCount(); i++) {
    const HxConfigKeyDef* item = &kHxConfigKeys[i];
    if (!ConfigStoreItemOverrideFromConfig(&snapshot, item)) {
      HX_LOGE("CFG", "save failed on key=%s", item->key);
      return false;
    }

    if (ConfigItemEqualsDefaultInConfig(&snapshot, item)) {
      default_count++;
    } else {
      overridden_count++;
    }
  }

  if (!HxNvsCommit(HX_NVS_STORE_CONFIG)) {
    HX_LOGE("CFG", "save commit failed");
    return false;
  }

  ConfigStateEnter();
  Hx.config_loaded = true;
  ConfigStateExit();

  HX_LOGI("CFG", "save complete overrides=%lu defaults=%lu",
          (unsigned long)overridden_count,
          (unsigned long)default_count);
  return true;
}

bool ConfigGetStorageInfo(HxConfigStorageInfo* out_info) {
  if (!out_info) {
    return false;
  }

  memset(out_info, 0, sizeof(*out_info));

  HxNvsStats stats{};
  if (!HxNvsGetStats(HX_NVS_STORE_CONFIG, &stats)) {
    return false;
  }

  HxConfig snapshot = {};
  ConfigCopyCurrent(&snapshot);

  size_t visible_count = 0;
  size_t writable_count = 0;
  size_t overridden_count = 0;

  for (size_t i = 0; i < ConfigKeyCount(); i++) {
    const HxConfigKeyDef* item = &kHxConfigKeys[i];
    if (item->console_visible) {
      visible_count++;
    }
    if (item->console_writable) {
      writable_count++;
    }
    if (!ConfigItemEqualsDefaultInConfig(&snapshot, item)) {
      overridden_count++;
    }
  }

  out_info->ready = g_config_ready;
  out_info->loaded = ConfigLoadedFlag();
  out_info->partition_label = stats.partition_label;
  out_info->namespace_name = stats.namespace_name;
  out_info->entry_size_bytes = HX_CONFIG_NVS_ENTRY_SIZE_BYTES;
  out_info->total_key_count = ConfigKeyCount();
  out_info->visible_key_count = visible_count;
  out_info->writable_key_count = writable_count;
  out_info->overridden_key_count = overridden_count;
  out_info->partition_entries_used = stats.used_entries;
  out_info->partition_entries_free = stats.free_entries;
  out_info->partition_entries_available = stats.available_entries;
  out_info->partition_entries_total = stats.total_entries;
  out_info->namespace_entries_used = stats.namespace_entries;
  return true;
}

bool ConfigFactoryFormat() {
  if (!g_config_ready) {
    HX_LOGE("CFG", "factory format failed, config store not ready");
    return false;
  }

  HX_LOGW("CFG", "factory format requested");
  if (!HxNvsFormat(HX_NVS_STORE_CONFIG)) {
    HX_LOGE("CFG", "factory format erase failed");
    return false;
  }

  HxConfig defaults = {};
  ConfigResetToDefaults(&defaults);
  ConfigCommitCurrent(&defaults, true);

  if (!ConfigSave()) {
    HX_LOGE("CFG", "factory format default save failed");
    return false;
  }

  HX_LOGW("CFG", "factory format complete");
  return true;
}

void ConfigApply() {
  HxConfig snapshot = {};
  ConfigCopyCurrent(&snapshot);

  LogSetLevel((HxLogLevel)snapshot.log_level);
  Hx.safeboot = snapshot.safeboot_enable;
}
