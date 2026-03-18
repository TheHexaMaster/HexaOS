/*
  HexaOS - config.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Runtime configuration service.
  Provides the persistent key-value configuration layer stored in NVS and used to load, query and save mutable HexaOS setup values across reboots.
*/

#include "hexaos.h"
#include "platform/esp_nvs.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static_assert(sizeof(HX_BUILD_DEFAULT_DEVICE_NAME) <= (HX_SETUP_DEVICE_NAME_MAX + 1),
              "HX_BUILD_DEFAULT_DEVICE_NAME is too long");

static bool g_setup_ready = false;

HxSetup HxSetupData = {};
const HxSetup HxSetupDefaults = {
  HX_BUILD_DEFAULT_DEVICE_NAME,
  (HxLogLevel)HX_BUILD_DEFAULT_LOG_LEVEL,
  (HX_BUILD_DEFAULT_SAFEBOOT_ENABLE != 0)
};

static const HxConfigKeyDef kHxConfigKeys[] = {
  {
    .key = HX_CFG_DEVICE_NAME,
    .type = HX_SCHEMA_VALUE_STRING,
    .setup_offset = offsetof(HxSetup, device_name),
    .value_size = sizeof(((HxSetup*)0)->device_name),
    .min_i32 = 0,
    .max_i32 = 0,
    .max_len = HX_SETUP_DEVICE_NAME_MAX,
    .console_visible = true,
    .console_writable = true
  },
  {
    .key = HX_CFG_LOG_LEVEL,
    .type = HX_SCHEMA_VALUE_LOG_LEVEL,
    .setup_offset = offsetof(HxSetup, log_level),
    .value_size = sizeof(((HxSetup*)0)->log_level),
    .min_i32 = (int32_t)HX_LOG_ERROR,
    .max_i32 = (int32_t)HX_LOG_DEBUG,
    .max_len = 0,
    .console_visible = true,
    .console_writable = true
  },
  {
    .key = HX_CFG_SAFEBOOT_ENABLE,
    .type = HX_SCHEMA_VALUE_BOOL,
    .setup_offset = offsetof(HxSetup, safeboot_enable),
    .value_size = sizeof(((HxSetup*)0)->safeboot_enable),
    .min_i32 = 0,
    .max_i32 = 1,
    .max_len = 0,
    .console_visible = true,
    .console_writable = true
  }
};

static bool SetupLogLevelIsValid(HxLogLevel level) {
  return (level >= HX_LOG_ERROR) && (level <= HX_LOG_DEBUG);
}

static void* SetupFieldPtr(HxSetup* setup, const HxConfigKeyDef* item) {
  if (!setup || !item) {
    return nullptr;
  }

  return (void*)(reinterpret_cast<uint8_t*>(setup) + item->setup_offset);
}

static const void* SetupFieldPtrConst(const HxSetup* setup, const HxConfigKeyDef* item) {
  if (!setup || !item) {
    return nullptr;
  }

  return (const void*)(reinterpret_cast<const uint8_t*>(setup) + item->setup_offset);
}

static bool SetupParseBoolText(const char* text, bool* value) {
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

static bool SetupParseInt32Text(const char* text, int32_t min_value, int32_t max_value, int32_t* value) {
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

static bool SetupAssignStringField(HxSetup* setup, const HxConfigKeyDef* item, const char* value) {
  if (!setup || !item || !value) {
    return false;
  }

  if ((item->type != HX_SCHEMA_VALUE_STRING) || (item->value_size == 0) || (item->max_len == 0)) {
    return false;
  }

  const size_t len = strlen(value);
  if ((len == 0) || (len > item->max_len) || (item->value_size < (len + 1))) {
    return false;
  }

  char* field = static_cast<char*>(SetupFieldPtr(setup, item));
  if (!field) {
    return false;
  }

  memset(field, 0, item->value_size);
  memcpy(field, value, len);
  field[len] = '\0';
  return true;
}

static bool SetupAssignBoolField(HxSetup* setup, const HxConfigKeyDef* item, bool value) {
  if (!setup || !item || (item->type != HX_SCHEMA_VALUE_BOOL) || (item->value_size != sizeof(bool))) {
    return false;
  }

  bool* field = static_cast<bool*>(SetupFieldPtr(setup, item));
  if (!field) {
    return false;
  }

  *field = value;
  return true;
}

static bool SetupAssignInt32Field(HxSetup* setup, const HxConfigKeyDef* item, int32_t value) {
  if (!setup || !item) {
    return false;
  }

  if ((value < item->min_i32) || (value > item->max_i32)) {
    return false;
  }

  if (item->type == HX_SCHEMA_VALUE_INT32) {
    if (item->value_size != sizeof(int32_t)) {
      return false;
    }

    int32_t* field = static_cast<int32_t*>(SetupFieldPtr(setup, item));
    if (!field) {
      return false;
    }

    *field = value;
    return true;
  }

  if (item->type == HX_SCHEMA_VALUE_LOG_LEVEL) {
    if ((item->value_size != sizeof(HxLogLevel)) || !SetupLogLevelIsValid((HxLogLevel)value)) {
      return false;
    }

    HxLogLevel* field = static_cast<HxLogLevel*>(SetupFieldPtr(setup, item));
    if (!field) {
      return false;
    }

    *field = (HxLogLevel)value;
    return true;
  }

  return false;
}

static bool SetupAssignValueFromString(HxSetup* setup, const HxConfigKeyDef* item, const char* value) {
  if (!setup || !item || !value) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING:
      return SetupAssignStringField(setup, item, value);

    case HX_SCHEMA_VALUE_BOOL: {
      bool parsed = false;
      if (!SetupParseBoolText(value, &parsed)) {
        return false;
      }
      return SetupAssignBoolField(setup, item, parsed);
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t parsed = 0;
      if (!SetupParseInt32Text(value, item->min_i32, item->max_i32, &parsed)) {
        return false;
      }
      return SetupAssignInt32Field(setup, item, parsed);
    }

    case HX_SCHEMA_VALUE_LOG_LEVEL: {
      HxLogLevel level;
      if (!SetupParseLogLevel(value, &level)) {
        return false;
      }
      return SetupAssignInt32Field(setup, item, (int32_t)level);
    }

    default:
      return false;
  }
}

static bool SetupReadItemFromNvs(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_STRING: {
      String text_value;
      if (!HxNvsGetString(HX_NVS_STORE_CONFIG, item->key, text_value)) {
        return true;
      }
      return SetupAssignStringField(&HxSetupData, item, text_value.c_str());
    }

    case HX_SCHEMA_VALUE_BOOL: {
      bool bool_value = false;
      if (!HxNvsGetBool(HX_NVS_STORE_CONFIG, item->key, &bool_value)) {
        return true;
      }
      return SetupAssignBoolField(&HxSetupData, item, bool_value);
    }

    case HX_SCHEMA_VALUE_INT32:
    case HX_SCHEMA_VALUE_LOG_LEVEL: {
      int32_t int_value = 0;
      if (!HxNvsGetInt(HX_NVS_STORE_CONFIG, item->key, &int_value)) {
        return true;
      }
      return SetupAssignInt32Field(&HxSetupData, item, int_value);
    }

    default:
      return false;
  }
}

static bool SetupItemEqualsDefault(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  const void* current_ptr = SetupFieldPtrConst(&HxSetupData, item);
  const void* default_ptr = SetupFieldPtrConst(&HxSetupDefaults, item);
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

    case HX_SCHEMA_VALUE_LOG_LEVEL:
      return (*static_cast<const HxLogLevel*>(current_ptr) == *static_cast<const HxLogLevel*>(default_ptr));

    default:
      return false;
  }
}

static bool SetupStoreItemOverride(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  if (SetupItemEqualsDefault(item)) {
    return HxNvsEraseKey(HX_NVS_STORE_CONFIG, item->key);
  }

  const void* current_ptr = SetupFieldPtrConst(&HxSetupData, item);
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

    case HX_SCHEMA_VALUE_LOG_LEVEL:
      return HxNvsSetInt(HX_NVS_STORE_CONFIG, item->key, (int32_t)(*static_cast<const HxLogLevel*>(current_ptr)));

    default:
      return false;
  }
}

size_t SetupConfigKeyCount() {
  return sizeof(kHxConfigKeys) / sizeof(kHxConfigKeys[0]);
}

const HxConfigKeyDef* SetupConfigKeyAt(size_t index) {
  if (index >= SetupConfigKeyCount()) {
    return nullptr;
  }

  return &kHxConfigKeys[index];
}

const HxConfigKeyDef* SetupFindConfigKey(const char* key) {
  if (!key || !key[0]) {
    return nullptr;
  }

  for (size_t i = 0; i < SetupConfigKeyCount(); i++) {
    if (strcmp(kHxConfigKeys[i].key, key) == 0) {
      return &kHxConfigKeys[i];
    }
  }

  return nullptr;
}

const char* SetupLogLevelText(HxLogLevel level) {
  switch (level) {
    case HX_LOG_ERROR: return "error";
    case HX_LOG_WARN:  return "warn";
    case HX_LOG_INFO:  return "info";
    case HX_LOG_DEBUG: return "debug";
    default:           return "unknown";
  }
}

bool SetupParseLogLevel(const char* text, HxLogLevel* level) {
  if (!text || !text[0] || !level) {
    return false;
  }

  if ((strcasecmp(text, "error") == 0) || (strcasecmp(text, "err") == 0)) {
    *level = HX_LOG_ERROR;
    return true;
  }

  if ((strcasecmp(text, "warn") == 0) || (strcasecmp(text, "warning") == 0) || (strcasecmp(text, "wrn") == 0)) {
    *level = HX_LOG_WARN;
    return true;
  }

  if ((strcasecmp(text, "info") == 0) || (strcasecmp(text, "inf") == 0)) {
    *level = HX_LOG_INFO;
    return true;
  }

  if ((strcasecmp(text, "debug") == 0) || (strcasecmp(text, "dbg") == 0)) {
    *level = HX_LOG_DEBUG;
    return true;
  }

  char* endptr = nullptr;
  long raw = strtol(text, &endptr, 10);
  if (endptr && (*endptr == '\0') && (raw >= (long)HX_LOG_ERROR) && (raw <= (long)HX_LOG_DEBUG)) {
    *level = (HxLogLevel)raw;
    return true;
  }

  return false;
}

void SetupResetToDefaults(HxSetup* setup) {
  if (!setup) {
    return;
  }

  *setup = HxSetupDefaults;
}

bool SetupConfigValueToString(const HxConfigKeyDef* item, char* out, size_t out_size) {
  if (!item || !out || (out_size == 0)) {
    return false;
  }

  out[0] = '\0';

  const void* ptr = SetupFieldPtrConst(&HxSetupData, item);
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

    case HX_SCHEMA_VALUE_LOG_LEVEL:
      snprintf(out, out_size, "%s", SetupLogLevelText(*static_cast<const HxLogLevel*>(ptr)));
      return true;

    default:
      return false;
  }
}

bool SetupConfigDefaultToString(const HxConfigKeyDef* item, char* out, size_t out_size) {
  if (!item || !out || (out_size == 0)) {
    return false;
  }

  out[0] = '\0';

  const void* ptr = SetupFieldPtrConst(&HxSetupDefaults, item);
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

    case HX_SCHEMA_VALUE_LOG_LEVEL:
      snprintf(out, out_size, "%s", SetupLogLevelText(*static_cast<const HxLogLevel*>(ptr)));
      return true;

    default:
      return false;
  }
}

bool SetupConfigSetValueFromString(const HxConfigKeyDef* item, const char* value) {
  if (!item || !value || !item->console_writable) {
    return false;
  }

  return SetupAssignValueFromString(&HxSetupData, item, value);
}

bool SetupConfigResetValue(const HxConfigKeyDef* item) {
  if (!item) {
    return false;
  }

  const void* default_ptr = SetupFieldPtrConst(&HxSetupDefaults, item);
  void* current_ptr = SetupFieldPtr(&HxSetupData, item);
  if (!default_ptr || !current_ptr || (item->value_size == 0)) {
    return false;
  }

  memcpy(current_ptr, default_ptr, item->value_size);
  return true;
}

bool SetupSetDeviceName(const char* value) {
  const HxConfigKeyDef* item = SetupFindConfigKey(HX_CFG_DEVICE_NAME);
  if (!item) {
    return false;
  }

  return SetupAssignStringField(&HxSetupData, item, value);
}

bool SetupSetLogLevel(HxLogLevel value) {
  const HxConfigKeyDef* item = SetupFindConfigKey(HX_CFG_LOG_LEVEL);
  if (!item) {
    return false;
  }

  return SetupAssignInt32Field(&HxSetupData, item, (int32_t)value);
}

bool SetupSetSafebootEnable(bool value) {
  const HxConfigKeyDef* item = SetupFindConfigKey(HX_CFG_SAFEBOOT_ENABLE);
  if (!item) {
    return false;
  }

  return SetupAssignBoolField(&HxSetupData, item, value);
}

bool SetupInit() {
  SetupResetToDefaults(&HxSetupData);
  g_setup_ready = EspNvsOpenConfig();
  return g_setup_ready;
}

bool SetupLoad() {
  SetupResetToDefaults(&HxSetupData);

  if (!g_setup_ready) {
    Hx.config_loaded = false;
    return false;
  }

  for (size_t i = 0; i < SetupConfigKeyCount(); i++) {
    if (!SetupReadItemFromNvs(&kHxConfigKeys[i])) {
      Hx.config_loaded = false;
      return false;
    }
  }

  Hx.config_loaded = true;
  return true;
}

bool SetupSave() {
  if (!g_setup_ready) {
    return false;
  }

  for (size_t i = 0; i < SetupConfigKeyCount(); i++) {
    if (!SetupStoreItemOverride(&kHxConfigKeys[i])) {
      return false;
    }
  }

  if (!HxNvsCommit(HX_NVS_STORE_CONFIG)) {
    return false;
  }

  Hx.config_loaded = true;
  return true;
}

void SetupApply() {
  LogSetLevel(HxSetupData.log_level);
  Hx.safeboot = HxSetupData.safeboot_enable;
}