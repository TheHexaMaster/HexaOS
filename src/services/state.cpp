/*
  HexaOS - state.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Runtime state persistence service.
  Maintains mutable non-configuration system state in NVS, including boot counters and other values that must survive reboots but are not treated as setup.
*/

#include "hexaos.h"
#include "platform/esp_nvs.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool g_state_ready = false;

static const HxStateKeyDef kHxStateKeys[] = {
  {
    .key = HX_STATE_BOOT_COUNT,
    .type = HX_SCHEMA_VALUE_INT32,
    .min_i32 = 0,
    .max_i32 = INT32_MAX,
    .max_len = 0,
    .console_visible = true
  },
  {
    .key = HX_STATE_LAST_RESET,
    .type = HX_SCHEMA_VALUE_STRING,
    .min_i32 = 0,
    .max_i32 = 0,
    .max_len = 32,
    .console_visible = true
  }
};

size_t StateKeyCount() {
  return sizeof(kHxStateKeys) / sizeof(kHxStateKeys[0]);
}

const HxStateKeyDef* StateKeyAt(size_t index) {
  if (index >= StateKeyCount()) {
    return nullptr;
  }

  return &kHxStateKeys[index];
}

const HxStateKeyDef* StateFindKey(const char* key) {
  if (!key || !key[0]) {
    return nullptr;
  }

  for (size_t i = 0; i < StateKeyCount(); i++) {
    if (strcmp(kHxStateKeys[i].key, key) == 0) {
      return &kHxStateKeys[i];
    }
  }

  return nullptr;
}

bool StateValueToString(const HxStateKeyDef* item, char* out, size_t out_size) {
  if (!item || !out || (out_size == 0) || !g_state_ready) {
    return false;
  }

  out[0] = '\0';

  switch (item->type) {
    case HX_SCHEMA_VALUE_BOOL: {
      bool value = false;
      if (!HxNvsGetBool(HX_NVS_STORE_STATE, item->key, &value)) {
        return false;
      }

      snprintf(out, out_size, "%s", value ? "true" : "false");
      return true;
    }

    case HX_SCHEMA_VALUE_INT32:
    case HX_SCHEMA_VALUE_LOG_LEVEL: {
      int32_t value = 0;
      if (!HxNvsGetInt(HX_NVS_STORE_STATE, item->key, &value)) {
        return false;
      }

      snprintf(out, out_size, "%ld", (long)value);
      return true;
    }

    case HX_SCHEMA_VALUE_STRING: {
      String text;
      if (!HxNvsGetString(HX_NVS_STORE_STATE, item->key, text)) {
        return false;
      }

      snprintf(out, out_size, "%s", text.c_str());
      return true;
    }

    default:
      return false;
  }
}

bool StateInit() {
  g_state_ready = EspNvsOpenState();
  return g_state_ready;
}

bool StateLoad() {
  if (!g_state_ready) {
    Hx.state_loaded = false;
    return false;
  }

  Hx.state_loaded = true;
  Hx.boot_count = StateGetInt(HX_STATE_BOOT_COUNT, 0) + 1;

  if (!StateSetInt(HX_STATE_BOOT_COUNT, (int32_t)Hx.boot_count)) {
    LogWarn("STA: boot_count store failed");
  }

  return true;
}

bool StateSave() {
  if (!g_state_ready) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateGetBool(const char* key, bool defval) {
  bool value = defval;
  if (!g_state_ready) {
    return defval;
  }

  if (HxNvsGetBool(HX_NVS_STORE_STATE, key, &value)) {
    return value;
  }

  return defval;
}

int32_t StateGetInt(const char* key, int32_t defval) {
  int32_t value = defval;
  if (!g_state_ready) {
    return defval;
  }

  if (HxNvsGetInt(HX_NVS_STORE_STATE, key, &value)) {
    return value;
  }

  return defval;
}

bool StateSetBool(const char* key, bool value) {
  if (!g_state_ready) {
    return false;
  }

  if (!HxNvsSetBool(HX_NVS_STORE_STATE, key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateSetInt(const char* key, int32_t value) {
  if (!g_state_ready) {
    return false;
  }

  if (!HxNvsSetInt(HX_NVS_STORE_STATE, key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}