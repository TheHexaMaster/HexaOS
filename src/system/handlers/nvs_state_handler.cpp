/*
  HexaOS - state.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Runtime state persistence service.
  Maintains mutable non-configuration system state in NVS, including boot
  counters and other values that must survive reboots but are not treated as
  config. The state layer supports both build-time static keys and runtime
  registered keys used by future modules, drivers and scripting layers.
*/

#include "hexaos.h"
#include "system/adapters/nvs_adapter.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static constexpr const char* HX_STATE_OWNER_STATIC = "system";
static constexpr const char* HX_STATE_OWNER_RUNTIME = "runtime";

static constexpr size_t HX_STATE_RUNTIME_MAX = 64;
static constexpr size_t HX_STATE_LOGICAL_KEY_MAX = 96;
static constexpr size_t HX_STATE_OWNER_MAX = 48;
static constexpr size_t HX_STATE_STRING_MAX = 256;
static constexpr size_t HX_STATE_STORAGE_KEY_SIZE = 15;

struct HxRuntimeStateSlot {
  bool used;
  HxStateKeyDef def;
  char* owned_key;
  char* owned_owner;
};

static bool g_state_ready = false;
static portMUX_TYPE g_state_registry_mux = portMUX_INITIALIZER_UNLOCKED;
static HxRuntimeStateSlot g_runtime_states[HX_STATE_RUNTIME_MAX];

static const HxStateKeyDef kHxStaticStateKeys[] = {
#define HX_STATE_ITEM(id, key_text, type_id, min_i32_value, max_i32_value, max_len_value, console_visible_value) \
  { \
    .key = key_text, \
    .type = type_id, \
    .min_i32 = (int32_t)(min_i32_value), \
    .max_i32 = (int32_t)(max_i32_value), \
    .max_len = (size_t)(max_len_value), \
    .flags = (uint16_t)(HX_STATE_FLAG_PERSISTENT | HX_STATE_FLAG_API_VISIBLE | ((console_visible_value) ? HX_STATE_FLAG_CONSOLE_VISIBLE : 0)), \
    .console_visible = (console_visible_value), \
    .owner = HX_STATE_OWNER_STATIC \
  },

  HX_STATE_SCHEMA(HX_STATE_ITEM)

#undef HX_STATE_ITEM
};

static constexpr size_t HX_STATIC_STATE_COUNT = sizeof(kHxStaticStateKeys) / sizeof(kHxStaticStateKeys[0]);

static bool StateIsValidKeyChar(char ch) {
  return ((ch >= 'a') && (ch <= 'z')) ||
         ((ch >= 'A') && (ch <= 'Z')) ||
         ((ch >= '0') && (ch <= '9')) ||
         (ch == '.') || (ch == '_') || (ch == '-');
}

static bool StateValidateLogicalKey(const char* key) {
  if (!key || !key[0]) {
    return false;
  }

  size_t len = strlen(key);
  if ((len == 0) || (len > HX_STATE_LOGICAL_KEY_MAX)) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    if (!StateIsValidKeyChar(key[i])) {
      return false;
    }
  }

  return true;
}

static bool StateValidateOwner(const char* owner) {
  if (!owner || !owner[0]) {
    return true;
  }

  return strlen(owner) <= HX_STATE_OWNER_MAX;
}

static bool StateValidateDef(const HxStateKeyDef* def) {
  if (!def) {
    return false;
  }

  if (!StateValidateLogicalKey(def->key)) {
    return false;
  }

  if (!StateValidateOwner(def->owner)) {
    return false;
  }

  switch (def->type) {
    case HX_SCHEMA_VALUE_BOOL:
      return true;

    case HX_SCHEMA_VALUE_INT32:
      return def->min_i32 <= def->max_i32;

    case HX_SCHEMA_VALUE_STRING:
      return (def->max_len > 0) && (def->max_len <= HX_STATE_STRING_MAX);

    default:
      return false;
  }
}

static uint64_t StateHashKey64(const char* text) {
  const uint64_t kOffset = 14695981039346656037ULL;
  const uint64_t kPrime = 1099511628211ULL;

  uint64_t hash = kOffset;

  while (text && *text) {
    hash ^= (uint8_t)(*text++);
    hash *= kPrime;
  }

  return hash;
}

static bool StateMakeStorageKey(const char* logical_key, char* out, size_t out_size) {
  static const char* kBase32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

  if (!logical_key || !out || (out_size < HX_STATE_STORAGE_KEY_SIZE)) {
    return false;
  }

  uint64_t hash = StateHashKey64(logical_key);

  out[0] = 's';
  for (int i = 13; i >= 1; i--) {
    out[i] = kBase32[hash & 0x1F];
    hash >>= 5;
  }
  out[14] = '\0';

  return true;
}

static const HxStateKeyDef* StateFindStaticKey(const char* key) {
  if (!key || !key[0]) {
    return nullptr;
  }

  for (size_t i = 0; i < HX_STATIC_STATE_COUNT; i++) {
    if (strcmp(kHxStaticStateKeys[i].key, key) == 0) {
      return &kHxStaticStateKeys[i];
    }
  }

  return nullptr;
}

static int StateFindRuntimeSlotIndexLocked(const char* key) {
  if (!key || !key[0]) {
    return -1;
  }

  for (size_t i = 0; i < HX_STATE_RUNTIME_MAX; i++) {
    if (!g_runtime_states[i].used) {
      continue;
    }

    if (strcmp(g_runtime_states[i].def.key, key) == 0) {
      return (int)i;
    }
  }

  return -1;
}

static const HxStateKeyDef* StateFindRuntimeKey(const char* key) {
  const HxStateKeyDef* found = nullptr;

  taskENTER_CRITICAL(&g_state_registry_mux);

  int index = StateFindRuntimeSlotIndexLocked(key);
  if (index >= 0) {
    found = &g_runtime_states[index].def;
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);
  return found;
}

static size_t StateRuntimeCountLocked() {
  size_t count = 0;

  for (size_t i = 0; i < HX_STATE_RUNTIME_MAX; i++) {
    if (g_runtime_states[i].used) {
      count++;
    }
  }

  return count;
}

static int StateFindFreeRuntimeSlotLocked() {
  for (size_t i = 0; i < HX_STATE_RUNTIME_MAX; i++) {
    if (!g_runtime_states[i].used) {
      return (int)i;
    }
  }

  return -1;
}

static void StateResetRuntimeRegistry() {
  taskENTER_CRITICAL(&g_state_registry_mux);

  for (size_t i = 0; i < HX_STATE_RUNTIME_MAX; i++) {
    if (g_runtime_states[i].owned_key) {
      free(g_runtime_states[i].owned_key);
    }

    if (g_runtime_states[i].owned_owner) {
      free(g_runtime_states[i].owned_owner);
    }

    memset(&g_runtime_states[i], 0, sizeof(g_runtime_states[i]));
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);
}

static bool StateResolveDefAndStorageKey(const char* key,
                                         const HxStateKeyDef** def_out,
                                         char* storage_key,
                                         size_t storage_key_size) {
  if (!def_out || !storage_key) {
    return false;
  }

  const HxStateKeyDef* def = StateFindKey(key);
  if (!def) {
    return false;
  }

  if (!StateMakeStorageKey(def->key, storage_key, storage_key_size)) {
    return false;
  }

  *def_out = def;
  return true;
}

static bool StateParseBoolText(const char* text, bool* value_out) {
  if (!text || !value_out) {
    return false;
  }

  if ((strcasecmp(text, "1") == 0) ||
      (strcasecmp(text, "true") == 0) ||
      (strcasecmp(text, "yes") == 0) ||
      (strcasecmp(text, "on") == 0)) {
    *value_out = true;
    return true;
  }

  if ((strcasecmp(text, "0") == 0) ||
      (strcasecmp(text, "false") == 0) ||
      (strcasecmp(text, "no") == 0) ||
      (strcasecmp(text, "off") == 0)) {
    *value_out = false;
    return true;
  }

  return false;
}

static bool StateParseIntText(const char* text, int32_t* value_out) {
  if (!text || !value_out) {
    return false;
  }

  errno = 0;

  char* endptr = nullptr;
  long value = strtol(text, &endptr, 10);

  if ((errno != 0) || (endptr == text)) {
    return false;
  }

  while (*endptr == ' ' || *endptr == '\t') {
    endptr++;
  }

  if (*endptr != '\0') {
    return false;
  }

  if ((value < INT32_MIN) || (value > INT32_MAX)) {
    return false;
  }

  *value_out = (int32_t)value;
  return true;
}

size_t StateKeyCount() {
  size_t runtime_count = 0;

  taskENTER_CRITICAL(&g_state_registry_mux);
  runtime_count = StateRuntimeCountLocked();
  taskEXIT_CRITICAL(&g_state_registry_mux);

  return HX_STATIC_STATE_COUNT + runtime_count;
}

const HxStateKeyDef* StateKeyAt(size_t index) {
  if (index < HX_STATIC_STATE_COUNT) {
    return &kHxStaticStateKeys[index];
  }

  size_t runtime_index = index - HX_STATIC_STATE_COUNT;
  const HxStateKeyDef* found = nullptr;

  taskENTER_CRITICAL(&g_state_registry_mux);

  size_t current = 0;
  for (size_t i = 0; i < HX_STATE_RUNTIME_MAX; i++) {
    if (!g_runtime_states[i].used) {
      continue;
    }

    if (current == runtime_index) {
      found = &g_runtime_states[i].def;
      break;
    }

    current++;
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);
  return found;
}

const HxStateKeyDef* StateFindKey(const char* key) {
  const HxStateKeyDef* item = StateFindStaticKey(key);
  if (item) {
    return item;
  }

  return StateFindRuntimeKey(key);
}

bool StateRegister(const HxStateKeyDef* def) {
  if (!StateValidateDef(def)) {
    return false;
  }

  if (StateFindKey(def->key)) {
    return false;
  }

  char* key_copy = (char*)malloc(strlen(def->key) + 1);
  if (!key_copy) {
    return false;
  }

  strcpy(key_copy, def->key);

  const char* owner_text = (def->owner && def->owner[0]) ? def->owner : HX_STATE_OWNER_RUNTIME;
  char* owner_copy = (char*)malloc(strlen(owner_text) + 1);
  if (!owner_copy) {
    free(key_copy);
    return false;
  }

  strcpy(owner_copy, owner_text);

  taskENTER_CRITICAL(&g_state_registry_mux);

  if (StateFindRuntimeSlotIndexLocked(def->key) >= 0) {
    taskEXIT_CRITICAL(&g_state_registry_mux);
    free(key_copy);
    free(owner_copy);
    return false;
  }

  int slot_index = StateFindFreeRuntimeSlotLocked();
  if (slot_index < 0) {
    taskEXIT_CRITICAL(&g_state_registry_mux);
    free(key_copy);
    free(owner_copy);
    return false;
  }

  HxRuntimeStateSlot* slot = &g_runtime_states[slot_index];
  memset(slot, 0, sizeof(*slot));

  slot->used = true;
  slot->owned_key = key_copy;
  slot->owned_owner = owner_copy;
  slot->def = *def;
  slot->def.key = slot->owned_key;
  slot->def.owner = slot->owned_owner;
  slot->def.flags |= (HX_STATE_FLAG_RUNTIME | HX_STATE_FLAG_PERSISTENT | HX_STATE_FLAG_API_VISIBLE);

  if (slot->def.console_visible || (slot->def.flags & HX_STATE_FLAG_CONSOLE_VISIBLE)) {
    slot->def.console_visible = true;
    slot->def.flags |= HX_STATE_FLAG_CONSOLE_VISIBLE;
  } else {
    slot->def.console_visible = false;
    slot->def.flags &= ~HX_STATE_FLAG_CONSOLE_VISIBLE;
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);
  return true;
}

bool StateUnregister(const char* key) {
  if (!key || !key[0]) {
    return false;
  }

  bool removed = false;

  taskENTER_CRITICAL(&g_state_registry_mux);

  int index = StateFindRuntimeSlotIndexLocked(key);
  if (index >= 0) {
    HxRuntimeStateSlot* slot = &g_runtime_states[index];

    if (slot->owned_key) {
      free(slot->owned_key);
    }

    if (slot->owned_owner) {
      free(slot->owned_owner);
    }

    memset(slot, 0, sizeof(*slot));
    removed = true;
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);
  return removed;
}

bool StateCreate(const char* key,
                 HxSchemaValueType type,
                 int32_t min_i32,
                 int32_t max_i32,
                 size_t max_len,
                 uint16_t flags,
                 bool console_visible,
                 const char* owner) {
  if (StateExists(key)) {
    return false;
  }

  HxStateKeyDef def = {
    .key = key,
    .type = type,
    .min_i32 = min_i32,
    .max_i32 = max_i32,
    .max_len = max_len,
    .flags = flags,
    .console_visible = console_visible,
    .owner = owner
  };

  if (!StateRegister(&def)) {
    return false;
  }

  switch (type) {
    case HX_SCHEMA_VALUE_BOOL: {
      bool current = false;
      if (!StateReadBool(key, &current)) {
        if (!StateSetBool(key, false)) {
          StateUnregister(key);
          return false;
        }
      }
      return true;
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t current = 0;
      if (!StateReadInt(key, &current)) {
        if (!StateSetInt(key, 0)) {
          StateUnregister(key);
          return false;
        }
      }
      return true;
    }

    case HX_SCHEMA_VALUE_STRING: {
      char current[2];
      if (!StateReadString(key, current, sizeof(current))) {
        if (!StateSetString(key, "")) {
          StateUnregister(key);
          return false;
        }
      }
      return true;
    }

    default:
      StateUnregister(key);
      return false;
  }
}

bool StateExists(const char* key) {
  return StateFindKey(key) != nullptr;
}

bool StateErase(const char* key) {
  if (!g_state_ready) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (def->flags & HX_STATE_FLAG_READONLY) {
    return false;
  }

  if (!HxNvsEraseKey(HX_NVS_STORE_STATE, storage_key)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateValueToString(const HxStateKeyDef* item, char* out, size_t out_size) {
  if (!item || !out || (out_size == 0) || !g_state_ready) {
    return false;
  }

  out[0] = '\0';

  switch (item->type) {
    case HX_SCHEMA_VALUE_BOOL: {
      bool value = false;
      if (!StateReadBool(item->key, &value)) {
        return false;
      }

      snprintf(out, out_size, "%s", value ? "true" : "false");
      return true;
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t value = 0;
      if (!StateReadInt(item->key, &value)) {
        return false;
      }

      snprintf(out, out_size, "%ld", (long)value);
      return true;
    }

    case HX_SCHEMA_VALUE_STRING:
      return StateReadString(item->key, out, out_size);

    default:
      return false;
  }
}

bool StateSetValueFromString(const HxStateKeyDef* item, const char* value) {
  if (!item || !value) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_BOOL: {
      bool parsed = false;
      if (!StateParseBoolText(value, &parsed)) {
        return false;
      }

      return StateSetBool(item->key, parsed);
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t parsed = 0;
      if (!StateParseIntText(value, &parsed)) {
        return false;
      }

      return StateSetInt(item->key, parsed);
    }

    case HX_SCHEMA_VALUE_STRING:
      return StateSetString(item->key, value);

    default:
      return false;
  }
}

bool StateWriteFromString(const char* key, const char* value) {
  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    return false;
  }

  return StateSetValueFromString(item, value);
}

bool StateInit() {
  StateResetRuntimeRegistry();
  g_state_ready = EspNvsOpenState();
  return g_state_ready;
}

bool StateLoad() {
  if (!g_state_ready) {
    Hx.state_loaded = false;
    return false;
  }

  Hx.state_loaded = true;
  Hx.boot_count = (uint32_t)(StateGetIntOr(HX_STATE_BOOT_COUNT, 0) + 1);

  if (!StateSetInt(HX_STATE_BOOT_COUNT, (int32_t)Hx.boot_count)) {
    LogWarn("STA: boot_count store failed");
  }

  return true;
}

bool StateCommit() {
  if (!g_state_ready) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateSave() {
  return StateCommit();
}

bool StateReadBool(const char* key, bool* value) {
  if (!g_state_ready || !value) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (def->type != HX_SCHEMA_VALUE_BOOL) {
    return false;
  }

  return HxNvsGetBool(HX_NVS_STORE_STATE, storage_key, value);
}

bool StateReadInt(const char* key, int32_t* value) {
  if (!g_state_ready || !value) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (def->type != HX_SCHEMA_VALUE_INT32) {
    return false;
  }

  if (!HxNvsGetInt(HX_NVS_STORE_STATE, storage_key, value)) {
    return false;
  }

  if ((*value < def->min_i32) || (*value > def->max_i32)) {
    return false;
  }

  return true;
}

bool StateReadString(const char* key, char* out, size_t out_size) {
  if (!g_state_ready || !out || (out_size == 0)) {
    return false;
  }

  out[0] = '\0';

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (def->type != HX_SCHEMA_VALUE_STRING) {
    return false;
  }

  String value;
  if (!HxNvsGetString(HX_NVS_STORE_STATE, storage_key, value)) {
    return false;
  }

  if (value.length() > def->max_len) {
    return false;
  }

  if ((value.length() + 1) > out_size) {
    return false;
  }

  memcpy(out, value.c_str(), value.length() + 1);
  return true;
}

bool StateGetBoolOr(const char* key, bool defval) {
  bool value = defval;

  if (StateReadBool(key, &value)) {
    return value;
  }

  return defval;
}

int32_t StateGetIntOr(const char* key, int32_t defval) {
  int32_t value = defval;

  if (StateReadInt(key, &value)) {
    return value;
  }

  return defval;
}

bool StateGetStringOr(const char* key, char* out, size_t out_size, const char* defval) {
  if (!out || (out_size == 0)) {
    return false;
  }

  if (StateReadString(key, out, out_size)) {
    return true;
  }

  if (!defval) {
    defval = "";
  }

  size_t len = strlen(defval);
  if ((len + 1) > out_size) {
    out[0] = '\0';
    return false;
  }

  memcpy(out, defval, len + 1);
  return false;
}

bool StateSetBool(const char* key, bool value) {
  if (!g_state_ready) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (def->type != HX_SCHEMA_VALUE_BOOL) {
    return false;
  }

  if (def->flags & HX_STATE_FLAG_READONLY) {
    return false;
  }

  if (!HxNvsSetBool(HX_NVS_STORE_STATE, storage_key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateSetInt(const char* key, int32_t value) {
  if (!g_state_ready) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (def->type != HX_SCHEMA_VALUE_INT32) {
    return false;
  }

  if (def->flags & HX_STATE_FLAG_READONLY) {
    return false;
  }

  if ((value < def->min_i32) || (value > def->max_i32)) {
    return false;
  }

  if (!HxNvsSetInt(HX_NVS_STORE_STATE, storage_key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateSetString(const char* key, const char* value) {
  if (!g_state_ready || !value) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (def->type != HX_SCHEMA_VALUE_STRING) {
    return false;
  }

  if (def->flags & HX_STATE_FLAG_READONLY) {
    return false;
  }

  size_t len = strlen(value);
  if (len > def->max_len) {
    return false;
  }

  if (!HxNvsSetString(HX_NVS_STORE_STATE, storage_key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateIncrementInt(const char* key, int32_t* new_value_out) {
  const HxStateKeyDef* def = StateFindKey(key);
  if (!def || (def->type != HX_SCHEMA_VALUE_INT32)) {
    return false;
  }

  int32_t current = 0;
  if (!StateReadInt(key, &current)) {
    current = 0;
  }

  if (current >= def->max_i32) {
    return false;
  }

  int32_t next = current + 1;
  if (!StateSetInt(key, next)) {
    return false;
  }

  if (new_value_out) {
    *new_value_out = next;
  }

  return true;
}

bool StateDecrementInt(const char* key, int32_t* new_value_out) {
  const HxStateKeyDef* def = StateFindKey(key);
  if (!def || (def->type != HX_SCHEMA_VALUE_INT32)) {
    return false;
  }

  int32_t current = 0;
  if (!StateReadInt(key, &current)) {
    current = 0;
  }

  if (current <= def->min_i32) {
    return false;
  }

  int32_t next = current - 1;
  if (!StateSetInt(key, next)) {
    return false;
  }

  if (new_value_out) {
    *new_value_out = next;
  }

  return true;
}

bool StateToggleBool(const char* key, bool* new_value_out) {
  const HxStateKeyDef* def = StateFindKey(key);
  if (!def || (def->type != HX_SCHEMA_VALUE_BOOL)) {
    return false;
  }

  bool current = false;
  if (!StateReadBool(key, &current)) {
    current = false;
  }

  bool next = !current;
  if (!StateSetBool(key, next)) {
    return false;
  }

  if (new_value_out) {
    *new_value_out = next;
  }

  return true;
}