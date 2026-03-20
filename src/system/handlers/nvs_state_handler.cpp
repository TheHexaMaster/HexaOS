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

static constexpr const char* HX_STATE_CATALOG_KEY = "CATALOG_STATE";

static constexpr size_t HX_STATE_RUNTIME_MAX = 64;
static constexpr size_t HX_STATE_LOGICAL_KEY_MAX = 96;
static constexpr size_t HX_STATE_STRING_MAX = 256;
static constexpr size_t HX_STATE_STORAGE_KEY_SIZE = 15;
static constexpr size_t HX_STATE_CATALOG_LINE_MAX = 256;
static constexpr size_t HX_STATE_CATALOG_RESERVE = 4096;
static constexpr size_t HX_STATE_PENDING_MAX = 96;
static constexpr size_t HX_STATE_NVS_ENTRY_SIZE = 32;

enum HxStatePendingKind : uint8_t {
  HX_STATE_PENDING_NONE = 0,
  HX_STATE_PENDING_ERASE = 1,
  HX_STATE_PENDING_BOOL = 2,
  HX_STATE_PENDING_INT32 = 3,
  HX_STATE_PENDING_STRING = 4
};

struct HxRuntimeStateSlot {
  bool used;
  HxStateKeyDef def;
  char* owned_key;
};

struct HxStatePendingSlot {
  bool used;
  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  HxStatePendingKind kind;
  uint32_t seq;
  bool bool_value;
  int32_t int_value;
  char* string_value;
};

struct HxStatePendingCommitItem {
  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  HxStatePendingKind kind;
  uint32_t seq;
  bool bool_value;
  int32_t int_value;
  char* string_value;
};

static bool g_state_ready = false;
static portMUX_TYPE g_state_registry_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE g_state_pending_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE g_state_commit_mux = portMUX_INITIALIZER_UNLOCKED;
static HxRuntimeStateSlot g_runtime_states[HX_STATE_RUNTIME_MAX];
static HxStatePendingSlot g_state_pending[HX_STATE_PENDING_MAX];
static bool g_state_commit_pending = false;
static uint32_t g_state_commit_deadline_ms = 0;
static uint32_t g_state_pending_seq = 0;

static void StateResetCommitWindow();
static uint32_t StateGetCommitDelayMs();
static bool StateValidateOwnerClass(HxStateOwnerClass owner_class);
static const char* StateOwnerClassText(HxStateOwnerClass owner_class);
static bool StateParseIntText(const char* text, int32_t* value_out);
static bool StateIsWriteAllowed(const HxStateKeyDef* def, HxStateWriteSource source);
static void StateArmCommitWindow();
static void StateRearmCommitWindow(uint32_t delay_ms);
static bool StateScheduleCommit();
static bool StateCommitIsDue();
static bool StateWriteRuntimeCatalog(bool commit);
static bool StateStageRuntimeCatalog();
static void StateClearPendingSlotLocked(HxStatePendingSlot* slot);
static void StateResetPendingBuffer();
static int StateFindPendingSlotIndexLocked(const char* storage_key);
static int StateFindFreePendingSlotLocked();
static uint32_t StateNextPendingSeqLocked();
static bool StateStagePendingBoolValue(const char* storage_key, bool value);
static bool StateStagePendingIntValue(const char* storage_key, int32_t value);
static bool StateStagePendingStringValue(const char* storage_key, const char* value);
static bool StateStagePendingEraseValue(const char* storage_key);
static bool StateEraseEx(const char* key, HxStateWriteSource source);
static bool StateSetValueFromStringEx(const HxStateKeyDef* item, const char* value, HxStateWriteSource source);
static bool StateSetBoolEx(const char* key, bool value, HxStateWriteSource source);
static bool StateSetIntEx(const char* key, int32_t value, HxStateWriteSource source);
static bool StateSetStringEx(const char* key, const char* value, HxStateWriteSource source);
static bool StateReadPendingBool(const char* storage_key, bool* handled, bool* value_out);
static bool StateReadPendingInt(const char* storage_key, bool* handled, int32_t* value_out);
static bool StateReadPendingString(const char* storage_key, bool* handled, char* out, size_t out_size, size_t max_len);
static void StateFreePendingCommitItems(HxStatePendingCommitItem* items, size_t count);
static bool StateClonePendingCommitItems(HxStatePendingCommitItem* items, size_t max_items, size_t* count_out);
static void StateClearCommittedPendingItems(const HxStatePendingCommitItem* items, size_t count);
static size_t StatePendingCountLocked();
bool StateCommit();

static const HxStateKeyDef kHxStaticStateKeys[] = {
#define HX_STATE_ITEM(id, key_text, type_id, min_i32_value, max_i32_value, max_len_value, console_visible_value, write_restricted_value) \
  { \
    .key = key_text, \
    .type = type_id, \
    .min_i32 = (int32_t)(min_i32_value), \
    .max_i32 = (int32_t)(max_i32_value), \
    .max_len = (size_t)(max_len_value), \
    .flags = (uint16_t)(HX_STATE_FLAG_PERSISTENT | HX_STATE_FLAG_API_VISIBLE | ((console_visible_value) ? HX_STATE_FLAG_CONSOLE_VISIBLE : 0) | ((write_restricted_value) ? HX_STATE_FLAG_WRITE_RESTRICTED : 0)), \
    .console_visible = (console_visible_value), \
    .owner_class = HX_STATE_OWNER_SYSTEM \
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

static bool StateValidateOwnerClass(HxStateOwnerClass owner_class) {
  return owner_class <= HX_STATE_OWNER_EXTERNAL;
}

static const char* StateOwnerClassText(HxStateOwnerClass owner_class) {
  switch (owner_class) {
    case HX_STATE_OWNER_SYSTEM:
      return "system";

    case HX_STATE_OWNER_KERNEL:
      return "kernel";

    case HX_STATE_OWNER_USER:
      return "user";

    case HX_STATE_OWNER_INTERNAL:
      return "internal";

    case HX_STATE_OWNER_EXTERNAL:
      return "external";

    default:
      return "unknown";
  }
}

static bool StateValidateDef(const HxStateKeyDef* def) {
  if (!def) {
    return false;
  }

  if (!StateValidateLogicalKey(def->key)) {
    return false;
  }

  if (!StateValidateOwnerClass(def->owner_class)) {
    return false;
  }

  if (def->owner_class == HX_STATE_OWNER_SYSTEM) {
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

    memset(&g_runtime_states[i], 0, sizeof(g_runtime_states[i]));
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);
  StateResetCommitWindow();
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


static bool StateIsWriteAllowed(const HxStateKeyDef* def, HxStateWriteSource source) {
  if (!def) {
    return false;
  }

  if ((def->flags & HX_STATE_FLAG_WRITE_RESTRICTED) && (source != HX_STATE_WRITE_SOURCE_SYSTEM)) {
    return false;
  }

  return true;
}

static uint32_t StateGetCommitDelayMs() {
  int32_t delay_ms = (int32_t)HX_CONFIG_DEFAULT_STATE_DELAY;

  const HxConfigKeyDef* item = ConfigFindConfigKey(HX_CFG_STATES_DELAY);
  if (item && (item->type == HX_SCHEMA_VALUE_INT32)) {
    if ((HxConfigData.states_delay >= item->min_i32) && (HxConfigData.states_delay <= item->max_i32)) {
      delay_ms = HxConfigData.states_delay;
    }
  }

  if (delay_ms < 0) {
    delay_ms = 0;
  }

  return (uint32_t)delay_ms;
}

static void StateResetCommitWindow() {
  taskENTER_CRITICAL(&g_state_commit_mux);
  g_state_commit_pending = false;
  g_state_commit_deadline_ms = 0;
  taskEXIT_CRITICAL(&g_state_commit_mux);
}

static void StateArmCommitWindow() {
  uint32_t now = millis();
  uint32_t delay_ms = StateGetCommitDelayMs();
  bool armed = false;

  taskENTER_CRITICAL(&g_state_commit_mux);

  if (!g_state_commit_pending) {
    g_state_commit_pending = true;
    g_state_commit_deadline_ms = now + delay_ms;
    armed = true;
  }

  taskEXIT_CRITICAL(&g_state_commit_mux);

  if (armed) {
    HX_LOGD("STA", "commit window armed delay=%lu ms", (unsigned long)delay_ms);
  }
}

static void StateRearmCommitWindow(uint32_t delay_ms) {
  taskENTER_CRITICAL(&g_state_commit_mux);
  g_state_commit_pending = true;
  g_state_commit_deadline_ms = millis() + delay_ms;
  taskEXIT_CRITICAL(&g_state_commit_mux);

  HX_LOGD("STA", "commit window rearmed delay=%lu ms", (unsigned long)delay_ms);
}

static bool StateCommitIsDue() {
  bool pending = false;
  uint32_t deadline_ms = 0;

  taskENTER_CRITICAL(&g_state_commit_mux);
  pending = g_state_commit_pending;
  deadline_ms = g_state_commit_deadline_ms;
  taskEXIT_CRITICAL(&g_state_commit_mux);

  if (!pending) {
    return false;
  }

  uint32_t now = millis();
  return (int32_t)(now - deadline_ms) >= 0;
}

static bool StateScheduleCommit() {
  if (!g_state_ready) {
    return false;
  }

  uint32_t delay_ms = StateGetCommitDelayMs();
  if (delay_ms == 0) {
    return StateCommit();
  }

  StateArmCommitWindow();
  return true;
}

static void StateClearPendingSlotLocked(HxStatePendingSlot* slot) {
  if (!slot) {
    return;
  }

  if (slot->string_value) {
    free(slot->string_value);
  }

  memset(slot, 0, sizeof(*slot));
}

static void StateResetPendingBuffer() {
  taskENTER_CRITICAL(&g_state_pending_mux);

  for (size_t i = 0; i < HX_STATE_PENDING_MAX; i++) {
    StateClearPendingSlotLocked(&g_state_pending[i]);
  }

  g_state_pending_seq = 0;

  taskEXIT_CRITICAL(&g_state_pending_mux);
}

static int StateFindPendingSlotIndexLocked(const char* storage_key) {
  if (!storage_key || !storage_key[0]) {
    return -1;
  }

  for (size_t i = 0; i < HX_STATE_PENDING_MAX; i++) {
    if (!g_state_pending[i].used) {
      continue;
    }

    if (strcmp(g_state_pending[i].storage_key, storage_key) == 0) {
      return (int)i;
    }
  }

  return -1;
}

static int StateFindFreePendingSlotLocked() {
  for (size_t i = 0; i < HX_STATE_PENDING_MAX; i++) {
    if (!g_state_pending[i].used) {
      return (int)i;
    }
  }

  return -1;
}

static size_t StatePendingCountLocked() {
  size_t count = 0;

  for (size_t i = 0; i < HX_STATE_PENDING_MAX; i++) {
    if (g_state_pending[i].used) {
      count++;
    }
  }

  return count;
}

static uint32_t StateNextPendingSeqLocked() {
  g_state_pending_seq++;
  if (g_state_pending_seq == 0) {
    g_state_pending_seq = 1;
  }

  return g_state_pending_seq;
}

static bool StateStagePendingBoolValue(const char* storage_key, bool value) {
  if (!storage_key || !storage_key[0]) {
    return false;
  }

  bool ok = false;

  taskENTER_CRITICAL(&g_state_pending_mux);

  int index = StateFindPendingSlotIndexLocked(storage_key);
  if (index < 0) {
    index = StateFindFreePendingSlotLocked();
  }

  if (index >= 0) {
    HxStatePendingSlot* slot = &g_state_pending[index];

    if (slot->used && slot->string_value) {
      free(slot->string_value);
      slot->string_value = nullptr;
    }

    if (!slot->used) {
      memset(slot, 0, sizeof(*slot));
      strncpy(slot->storage_key, storage_key, sizeof(slot->storage_key) - 1);
      slot->storage_key[sizeof(slot->storage_key) - 1] = '\0';
      slot->used = true;
    }

    slot->kind = HX_STATE_PENDING_BOOL;
    slot->bool_value = value;
    slot->seq = StateNextPendingSeqLocked();
    ok = true;
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);
  return ok;
}

static bool StateStagePendingIntValue(const char* storage_key, int32_t value) {
  if (!storage_key || !storage_key[0]) {
    return false;
  }

  bool ok = false;

  taskENTER_CRITICAL(&g_state_pending_mux);

  int index = StateFindPendingSlotIndexLocked(storage_key);
  if (index < 0) {
    index = StateFindFreePendingSlotLocked();
  }

  if (index >= 0) {
    HxStatePendingSlot* slot = &g_state_pending[index];

    if (slot->used && slot->string_value) {
      free(slot->string_value);
      slot->string_value = nullptr;
    }

    if (!slot->used) {
      memset(slot, 0, sizeof(*slot));
      strncpy(slot->storage_key, storage_key, sizeof(slot->storage_key) - 1);
      slot->storage_key[sizeof(slot->storage_key) - 1] = '\0';
      slot->used = true;
    }

    slot->kind = HX_STATE_PENDING_INT32;
    slot->int_value = value;
    slot->seq = StateNextPendingSeqLocked();
    ok = true;
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);
  return ok;
}

static bool StateStagePendingStringValue(const char* storage_key, const char* value) {
  if (!storage_key || !storage_key[0] || !value) {
    return false;
  }

  char* value_copy = (char*)malloc(strlen(value) + 1);
  if (!value_copy) {
    return false;
  }

  strcpy(value_copy, value);

  bool ok = false;

  taskENTER_CRITICAL(&g_state_pending_mux);

  int index = StateFindPendingSlotIndexLocked(storage_key);
  if (index < 0) {
    index = StateFindFreePendingSlotLocked();
  }

  if (index >= 0) {
    HxStatePendingSlot* slot = &g_state_pending[index];

    if (slot->used && slot->string_value) {
      free(slot->string_value);
      slot->string_value = nullptr;
    }

    if (!slot->used) {
      memset(slot, 0, sizeof(*slot));
      strncpy(slot->storage_key, storage_key, sizeof(slot->storage_key) - 1);
      slot->storage_key[sizeof(slot->storage_key) - 1] = '\0';
      slot->used = true;
    }

    slot->kind = HX_STATE_PENDING_STRING;
    slot->string_value = value_copy;
    slot->seq = StateNextPendingSeqLocked();
    ok = true;
    value_copy = nullptr;
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);

  if (value_copy) {
    free(value_copy);
  }

  return ok;
}

static bool StateStagePendingEraseValue(const char* storage_key) {
  if (!storage_key || !storage_key[0]) {
    return false;
  }

  bool ok = false;

  taskENTER_CRITICAL(&g_state_pending_mux);

  int index = StateFindPendingSlotIndexLocked(storage_key);
  if (index < 0) {
    index = StateFindFreePendingSlotLocked();
  }

  if (index >= 0) {
    HxStatePendingSlot* slot = &g_state_pending[index];

    if (slot->used && slot->string_value) {
      free(slot->string_value);
      slot->string_value = nullptr;
    }

    if (!slot->used) {
      memset(slot, 0, sizeof(*slot));
      strncpy(slot->storage_key, storage_key, sizeof(slot->storage_key) - 1);
      slot->storage_key[sizeof(slot->storage_key) - 1] = '\0';
      slot->used = true;
    }

    slot->kind = HX_STATE_PENDING_ERASE;
    slot->seq = StateNextPendingSeqLocked();
    ok = true;
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);
  return ok;
}

static bool StateReadPendingBool(const char* storage_key, bool* handled, bool* value_out) {
  if (handled) {
    *handled = false;
  }

  if (!storage_key || !value_out) {
    return false;
  }

  bool found = false;
  bool ok = false;
  bool value = false;

  taskENTER_CRITICAL(&g_state_pending_mux);

  int index = StateFindPendingSlotIndexLocked(storage_key);
  if (index >= 0) {
    found = true;
    if (g_state_pending[index].kind == HX_STATE_PENDING_BOOL) {
      value = g_state_pending[index].bool_value;
      ok = true;
    }
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);

  if (handled) {
    *handled = found;
  }

  if (ok) {
    *value_out = value;
  }

  return ok;
}

static bool StateReadPendingInt(const char* storage_key, bool* handled, int32_t* value_out) {
  if (handled) {
    *handled = false;
  }

  if (!storage_key || !value_out) {
    return false;
  }

  bool found = false;
  bool ok = false;
  int32_t value = 0;

  taskENTER_CRITICAL(&g_state_pending_mux);

  int index = StateFindPendingSlotIndexLocked(storage_key);
  if (index >= 0) {
    found = true;
    if (g_state_pending[index].kind == HX_STATE_PENDING_INT32) {
      value = g_state_pending[index].int_value;
      ok = true;
    }
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);

  if (handled) {
    *handled = found;
  }

  if (ok) {
    *value_out = value;
  }

  return ok;
}

static bool StateReadPendingString(const char* storage_key, bool* handled, char* out, size_t out_size, size_t max_len) {
  if (handled) {
    *handled = false;
  }

  if (!storage_key || !out || (out_size == 0)) {
    return false;
  }

  out[0] = '\0';

  bool found = false;
  bool ok = false;

  taskENTER_CRITICAL(&g_state_pending_mux);

  int index = StateFindPendingSlotIndexLocked(storage_key);
  if (index >= 0) {
    found = true;

    if ((g_state_pending[index].kind == HX_STATE_PENDING_STRING) && g_state_pending[index].string_value) {
      size_t len = strlen(g_state_pending[index].string_value);
      if ((len <= max_len) && ((len + 1) <= out_size)) {
        memcpy(out, g_state_pending[index].string_value, len + 1);
        ok = true;
      }
    }
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);

  if (handled) {
    *handled = found;
  }

  return ok;
}

static void StateFreePendingCommitItems(HxStatePendingCommitItem* items, size_t count) {
  if (!items) {
    return;
  }

  for (size_t i = 0; i < count; i++) {
    if (items[i].string_value) {
      free(items[i].string_value);
      items[i].string_value = nullptr;
    }
  }
}

static bool StateClonePendingCommitItems(HxStatePendingCommitItem* items, size_t max_items, size_t* count_out) {
  if (!items || !count_out) {
    return false;
  }

  *count_out = 0;

  taskENTER_CRITICAL(&g_state_pending_mux);

  for (size_t i = 0; i < HX_STATE_PENDING_MAX; i++) {
    if (!g_state_pending[i].used) {
      continue;
    }

    if (*count_out >= max_items) {
      taskEXIT_CRITICAL(&g_state_pending_mux);
      StateFreePendingCommitItems(items, *count_out);
      return false;
    }

    HxStatePendingCommitItem* item = &items[*count_out];
    memset(item, 0, sizeof(*item));
    strncpy(item->storage_key, g_state_pending[i].storage_key, sizeof(item->storage_key) - 1);
    item->storage_key[sizeof(item->storage_key) - 1] = '\0';
    item->kind = g_state_pending[i].kind;
    item->seq = g_state_pending[i].seq;
    item->bool_value = g_state_pending[i].bool_value;
    item->int_value = g_state_pending[i].int_value;

    if ((g_state_pending[i].kind == HX_STATE_PENDING_STRING) && g_state_pending[i].string_value) {
      item->string_value = (char*)malloc(strlen(g_state_pending[i].string_value) + 1);
      if (!item->string_value) {
        taskEXIT_CRITICAL(&g_state_pending_mux);
        StateFreePendingCommitItems(items, *count_out + 1);
        return false;
      }

      strcpy(item->string_value, g_state_pending[i].string_value);
    }

    (*count_out)++;
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);
  return true;
}

static void StateClearCommittedPendingItems(const HxStatePendingCommitItem* items, size_t count) {
  if (!items) {
    return;
  }

  taskENTER_CRITICAL(&g_state_pending_mux);

  for (size_t i = 0; i < count; i++) {
    int index = StateFindPendingSlotIndexLocked(items[i].storage_key);
    if (index < 0) {
      continue;
    }

    if (g_state_pending[index].seq != items[i].seq) {
      continue;
    }

    StateClearPendingSlotLocked(&g_state_pending[index]);
  }

  taskEXIT_CRITICAL(&g_state_pending_mux);
}

static bool StateStageRuntimeCatalog() {
  if (!StateWriteRuntimeCatalog(false)) {
    return false;
  }

  return StateScheduleCommit();
}

static bool StateDefsCompatible(const HxStateKeyDef* existing, const HxStateKeyDef* requested) {
  if (!existing || !requested) {
    return false;
  }

  if (strcmp(existing->key, requested->key) != 0) {
    return false;
  }

  if (existing->type != requested->type) {
    return false;
  }

  if (existing->console_visible != requested->console_visible) {
    return false;
  }

  if (existing->owner_class != requested->owner_class) {
    return false;
  }

  constexpr uint16_t kComparableFlags = HX_STATE_FLAG_WRITE_RESTRICTED;
  if ((existing->flags & kComparableFlags) != (requested->flags & kComparableFlags)) {
    return false;
  }

  switch (existing->type) {
    case HX_SCHEMA_VALUE_BOOL:
      return true;

    case HX_SCHEMA_VALUE_INT32:
      return (existing->min_i32 == requested->min_i32) &&
             (existing->max_i32 == requested->max_i32);

    case HX_SCHEMA_VALUE_STRING:
      return existing->max_len == requested->max_len;

    default:
      return false;
  }
}

static void StateRemoveRuntimeSlotLocked(int index) {
  if ((index < 0) || (index >= (int)HX_STATE_RUNTIME_MAX)) {
    return;
  }

  HxRuntimeStateSlot* slot = &g_runtime_states[index];

  if (slot->owned_key) {
    free(slot->owned_key);
  }

  memset(slot, 0, sizeof(*slot));
}

static bool StateInsertRuntimeDef(const HxStateKeyDef* def, bool persist_catalog) {
  if (!StateValidateDef(def)) {
    return false;
  }

  if (StateFindStaticKey(def->key)) {
    return false;
  }

  if (StateFindRuntimeKey(def->key)) {
    return false;
  }

  char* key_copy = (char*)malloc(strlen(def->key) + 1);
  if (!key_copy) {
    return false;
  }

  strcpy(key_copy, def->key);

  int slot_index = -1;

  taskENTER_CRITICAL(&g_state_registry_mux);

  if (StateFindRuntimeSlotIndexLocked(def->key) >= 0) {
    taskEXIT_CRITICAL(&g_state_registry_mux);
    free(key_copy);
    return false;
  }

  slot_index = StateFindFreeRuntimeSlotLocked();
  if (slot_index < 0) {
    taskEXIT_CRITICAL(&g_state_registry_mux);
    free(key_copy);
    return false;
  }

  HxRuntimeStateSlot* slot = &g_runtime_states[slot_index];
  memset(slot, 0, sizeof(*slot));

  slot->used = true;
  slot->owned_key = key_copy;
  slot->def = *def;
  slot->def.key = slot->owned_key;
  slot->def.flags |= (HX_STATE_FLAG_RUNTIME | HX_STATE_FLAG_PERSISTENT | HX_STATE_FLAG_API_VISIBLE);

  if (slot->def.console_visible || (slot->def.flags & HX_STATE_FLAG_CONSOLE_VISIBLE)) {
    slot->def.console_visible = true;
    slot->def.flags |= HX_STATE_FLAG_CONSOLE_VISIBLE;
  } else {
    slot->def.console_visible = false;
    slot->def.flags &= ~HX_STATE_FLAG_CONSOLE_VISIBLE;
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);

  HX_LOGD("STA", "register runtime key=%s owner=%s", slot->def.key, StateOwnerClassText(slot->def.owner_class));

  if (persist_catalog && !StateStageRuntimeCatalog()) {
    taskENTER_CRITICAL(&g_state_registry_mux);
    int rollback_index = StateFindRuntimeSlotIndexLocked(def->key);
    if (rollback_index >= 0) {
      StateRemoveRuntimeSlotLocked(rollback_index);
    }
    taskEXIT_CRITICAL(&g_state_registry_mux);
    return false;
  }

  return true;
}

static bool StateBuildRuntimeCatalog(String& manifest) {
  manifest = "";
  manifest.reserve(HX_STATE_CATALOG_RESERVE);

  for (size_t i = 0; i < HX_STATE_RUNTIME_MAX; i++) {
    bool used = false;
    HxStateKeyDef def_copy{};
    char key_copy[HX_STATE_LOGICAL_KEY_MAX + 1];

    taskENTER_CRITICAL(&g_state_registry_mux);

    if (g_runtime_states[i].used) {
      used = true;
      def_copy = g_runtime_states[i].def;

      key_copy[0] = '\0';

      if (g_runtime_states[i].def.key) {
        strncpy(key_copy, g_runtime_states[i].def.key, sizeof(key_copy) - 1);
        key_copy[sizeof(key_copy) - 1] = '\0';
      }
    }

    taskEXIT_CRITICAL(&g_state_registry_mux);

    if (!used) {
      continue;
    }

    char line[HX_STATE_CATALOG_LINE_MAX];
    int written = snprintf(line,
                           sizeof(line),
                           "%s|%d|%ld|%ld|%lu|%u|%u|%u\n",
                           key_copy,
                           (int)def_copy.type,
                           (long)def_copy.min_i32,
                           (long)def_copy.max_i32,
                           (unsigned long)def_copy.max_len,
                           (unsigned int)def_copy.flags,
                           def_copy.console_visible ? 1U : 0U,
                           (unsigned int)def_copy.owner_class);

    if ((written <= 0) || ((size_t)written >= sizeof(line))) {
      return false;
    }

    manifest += line;
  }

  return true;
}

static bool StateWriteRuntimeCatalog(bool commit) {
  if (!g_state_ready) {
    return false;
  }

  String manifest;
  if (!StateBuildRuntimeCatalog(manifest)) {
    return false;
  }

  bool ok = false;

  if (manifest.length() == 0) {
    ok = StateStagePendingEraseValue(HX_STATE_CATALOG_KEY);
  } else {
    ok = StateStagePendingStringValue(HX_STATE_CATALOG_KEY, manifest.c_str());
  }

  if (!ok) {
    return false;
  }

  HX_LOGD("STA", "stage runtime catalog len=%lu", (unsigned long)manifest.length());

  if (commit) {
    return StateCommit();
  }

  return true;
}

static bool StateLoadRuntimeCatalog() {
  if (!g_state_ready) {
    return false;
  }

  String manifest;
  if (!HxNvsGetString(HX_NVS_STORE_STATE, HX_STATE_CATALOG_KEY, manifest)) {
    return true;
  }

  char* buffer = (char*)malloc(manifest.length() + 1);
  if (!buffer) {
    return false;
  }

  memcpy(buffer, manifest.c_str(), manifest.length() + 1);

  char* line_ctx = nullptr;

  for (char* line = strtok_r(buffer, "\n", &line_ctx);
       line != nullptr;
       line = strtok_r(nullptr, "\n", &line_ctx)) {

    size_t len = strlen(line);
    while ((len > 0) && ((line[len - 1] == '\r') || (line[len - 1] == '\n'))) {
      line[--len] = '\0';
    }

    if (!line[0]) {
      continue;
    }

    char* fields[8] = { nullptr };
    size_t field_count = 0;
    char* field_ctx = nullptr;

    for (char* tok = strtok_r(line, "|", &field_ctx);
         tok != nullptr && field_count < 8;
         tok = strtok_r(nullptr, "|", &field_ctx)) {
      fields[field_count++] = tok;
    }

    if (field_count != 8) {
      continue;
    }

    int32_t type_i32 = 0;
    int32_t min_i32 = 0;
    int32_t max_i32 = 0;
    int32_t max_len_i32 = 0;
    int32_t flags_i32 = 0;
    int32_t console_visible_i32 = 0;
    int32_t owner_class_i32 = 0;

    if (!StateParseIntText(fields[1], &type_i32) ||
        !StateParseIntText(fields[2], &min_i32) ||
        !StateParseIntText(fields[3], &max_i32) ||
        !StateParseIntText(fields[4], &max_len_i32) ||
        !StateParseIntText(fields[5], &flags_i32) ||
        !StateParseIntText(fields[6], &console_visible_i32) ||
        !StateParseIntText(fields[7], &owner_class_i32)) {
      continue;
    }

    if ((owner_class_i32 < (int32_t)HX_STATE_OWNER_SYSTEM) ||
        (owner_class_i32 > (int32_t)HX_STATE_OWNER_EXTERNAL)) {
      continue;
    }

    HxStateKeyDef def = {
      .key = fields[0],
      .type = (HxSchemaValueType)type_i32,
      .min_i32 = min_i32,
      .max_i32 = max_i32,
      .max_len = (size_t)max_len_i32,
      .flags = (uint16_t)flags_i32,
      .console_visible = (console_visible_i32 != 0),
      .owner_class = (HxStateOwnerClass)owner_class_i32
    };

    def.flags |= (HX_STATE_FLAG_RUNTIME | HX_STATE_FLAG_PERSISTENT | HX_STATE_FLAG_API_VISIBLE);

    if (def.console_visible) {
      def.flags |= HX_STATE_FLAG_CONSOLE_VISIBLE;
    } else {
      def.flags &= ~HX_STATE_FLAG_CONSOLE_VISIBLE;
    }

    if (!StateValidateDef(&def)) {
      continue;
    }

    if (StateFindStaticKey(def.key)) {
      continue;
    }

    if (StateFindRuntimeKey(def.key)) {
      continue;
    }

    if (!StateInsertRuntimeDef(&def, false)) {
      continue;
    }
  }

  free(buffer);

  size_t runtime_count = 0;
  taskENTER_CRITICAL(&g_state_registry_mux);
  runtime_count = StateRuntimeCountLocked();
  taskEXIT_CRITICAL(&g_state_registry_mux);

  HX_LOGD("STA", "runtime catalog loaded count=%lu",
          (unsigned long)runtime_count);

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
  return StateInsertRuntimeDef(def, true);
}

bool StateUnregister(const char* key) {
  return StateDelete(key);
}

bool StateCreate(const char* key,
                 HxSchemaValueType type,
                 int32_t min_i32,
                 int32_t max_i32,
                 size_t max_len,
                 uint16_t flags,
                 bool console_visible,
                 HxStateOwnerClass owner_class) {
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
    .owner_class = owner_class
  };

  if (!StateRegister(&def)) {
    return false;
  }

  switch (type) {
    case HX_SCHEMA_VALUE_BOOL: {
      bool current = false;
      if (!StateReadBool(key, &current)) {
        if (!StateSetBoolEx(key, false, HX_STATE_WRITE_SOURCE_SYSTEM)) {
          StateDelete(key);
          return false;
        }
      }
      return true;
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t current = 0;
      if (!StateReadInt(key, &current)) {
        int32_t initial_value = 0;
        if (initial_value < min_i32) {
          initial_value = min_i32;
        } else if (initial_value > max_i32) {
          initial_value = max_i32;
        }

        if (!StateSetIntEx(key, initial_value, HX_STATE_WRITE_SOURCE_SYSTEM)) {
          StateDelete(key);
          return false;
        }
      }
      return true;
    }

    case HX_SCHEMA_VALUE_STRING: {
      char current[2];
      if (!StateReadString(key, current, sizeof(current))) {
        if (!StateSetStringEx(key, "", HX_STATE_WRITE_SOURCE_SYSTEM)) {
          StateDelete(key);
          return false;
        }
      }
      return true;
    }

    default:
      StateDelete(key);
      return false;
  }
}

bool StateEnsure(const char* key,
                 HxSchemaValueType type,
                 int32_t min_i32,
                 int32_t max_i32,
                 size_t max_len,
                 uint16_t flags,
                 bool console_visible,
                 HxStateOwnerClass owner_class) {
  HxStateKeyDef requested = {
    .key = key,
    .type = type,
    .min_i32 = min_i32,
    .max_i32 = max_i32,
    .max_len = max_len,
    .flags = flags,
    .console_visible = console_visible,
    .owner_class = owner_class
  };

  const HxStateKeyDef* existing = StateFindKey(key);
  if (existing) {
    return StateDefsCompatible(existing, &requested);
  }

  return StateCreate(key, type, min_i32, max_i32, max_len, flags, console_visible, owner_class);
}

bool StateDelete(const char* key) {
  if (!g_state_ready || !key || !key[0]) {
    return false;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    return false;
  }

  if ((item->flags & HX_STATE_FLAG_RUNTIME) == 0) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  if (!StateMakeStorageKey(item->key, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (!StateStagePendingEraseValue(storage_key)) {
    return false;
  }

  taskENTER_CRITICAL(&g_state_registry_mux);

  int index = StateFindRuntimeSlotIndexLocked(key);
  if (index >= 0) {
    StateRemoveRuntimeSlotLocked(index);
  }

  taskEXIT_CRITICAL(&g_state_registry_mux);

  HX_LOGD("STA", "delete runtime key=%s", key);

  if (!StateStageRuntimeCatalog()) {
    StateScheduleCommit();
    return false;
  }

  return true;
}

bool StateExists(const char* key) {
  return StateFindKey(key) != nullptr;
}

static bool StateEraseEx(const char* key, HxStateWriteSource source) {
  if (!g_state_ready) {
    return false;
  }

  char storage_key[HX_STATE_STORAGE_KEY_SIZE];
  const HxStateKeyDef* def = nullptr;

  if (!StateResolveDefAndStorageKey(key, &def, storage_key, sizeof(storage_key))) {
    return false;
  }

  if (!StateIsWriteAllowed(def, source)) {
    return false;
  }

  if (!StateStagePendingEraseValue(storage_key)) {
    return false;
  }

  HX_LOGD("STA", "stage erase key=%s", def->key);
  return StateScheduleCommit();
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

static bool StateSetValueFromStringEx(const HxStateKeyDef* item, const char* value, HxStateWriteSource source) {
  if (!item || !value) {
    return false;
  }

  switch (item->type) {
    case HX_SCHEMA_VALUE_BOOL: {
      bool parsed = false;
      if (!StateParseBoolText(value, &parsed)) {
        return false;
      }

      return StateSetBoolEx(item->key, parsed, source);
    }

    case HX_SCHEMA_VALUE_INT32: {
      int32_t parsed = 0;
      if (!StateParseIntText(value, &parsed)) {
        return false;
      }

      return StateSetIntEx(item->key, parsed, source);
    }

    case HX_SCHEMA_VALUE_STRING:
      return StateSetStringEx(item->key, value, source);

    default:
      return false;
  }
}

bool StateWriteFromString(const char* key, const char* value) {
  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    return false;
  }

  return StateSetValueFromStringEx(item, value, HX_STATE_WRITE_SOURCE_USER);
}

bool StateInit() {
  StateResetRuntimeRegistry();
  StateResetPendingBuffer();

  g_state_ready = EspNvsOpenState();
  if (!g_state_ready) {
    return false;
  }

  if (!StateLoadRuntimeCatalog()) {
    return false;
  }

  StateResetCommitWindow();
  return true;
}

bool StateLoad() {
  if (!g_state_ready) {
    Hx.state_loaded = false;
    return false;
  }

  Hx.state_loaded = true;
  Hx.boot_count = (uint32_t)(StateGetIntOr(HX_STATE_BOOT_COUNT, 0) + 1);

  if (!StateSetIntEx(HX_STATE_BOOT_COUNT, (int32_t)Hx.boot_count, HX_STATE_WRITE_SOURCE_SYSTEM)) {
    LogWarn("STA: boot_count store failed");
  }

  return true;
}

bool StateCommit() {
  if (!g_state_ready) {
    return false;
  }

  HxStatePendingCommitItem items[HX_STATE_PENDING_MAX];
  memset(items, 0, sizeof(items));

  size_t count = 0;
  if (!StateClonePendingCommitItems(items, HX_STATE_PENDING_MAX, &count)) {
    HX_LOGW("STA", "commit snapshot failed");
    return false;
  }

  if (count == 0) {
    StateResetCommitWindow();
    return true;
  }

  HX_LOGD("STA", "commit flush start ops=%lu", (unsigned long)count);

  for (size_t i = 0; i < count; i++) {
    bool ok = false;

    switch (items[i].kind) {
      case HX_STATE_PENDING_ERASE:
        ok = HxNvsEraseKey(HX_NVS_STORE_STATE, items[i].storage_key);
        break;

      case HX_STATE_PENDING_BOOL:
        ok = HxNvsSetBool(HX_NVS_STORE_STATE, items[i].storage_key, items[i].bool_value);
        break;

      case HX_STATE_PENDING_INT32:
        ok = HxNvsSetInt(HX_NVS_STORE_STATE, items[i].storage_key, items[i].int_value);
        break;

      case HX_STATE_PENDING_STRING:
        ok = items[i].string_value && HxNvsSetString(HX_NVS_STORE_STATE, items[i].storage_key, items[i].string_value);
        break;

      default:
        ok = false;
        break;
    }

    if (!ok) {
      HX_LOGW("STA", "commit apply failed key=%s", items[i].storage_key);
      StateFreePendingCommitItems(items, count);
      return false;
    }
  }

  if (!HxNvsCommit(HX_NVS_STORE_STATE)) {
    HX_LOGW("STA", "commit flush failed");
    StateFreePendingCommitItems(items, count);
    return false;
  }

  StateClearCommittedPendingItems(items, count);
  StateFreePendingCommitItems(items, count);

  size_t remaining = 0;
  taskENTER_CRITICAL(&g_state_pending_mux);
  remaining = StatePendingCountLocked();
  taskEXIT_CRITICAL(&g_state_pending_mux);

  if (remaining == 0) {
    StateResetCommitWindow();
  } else {
    StateRearmCommitWindow(StateGetCommitDelayMs());
  }

  HX_LOGD("STA", "commit flush OK ops=%lu remaining=%lu",
          (unsigned long)count,
          (unsigned long)remaining);
  return true;
}

void StateLoop() {
  if (!g_state_ready) {
    return;
  }

  if (!StateCommitIsDue()) {
    return;
  }

  if (!StateCommit()) {
    HX_LOGW("STA", "delayed commit failed");
  }
}

bool StateSave() {
  return StateCommit();
}

bool StateFormat() {
  if (!g_state_ready) {
    return false;
  }

  StateResetPendingBuffer();
  StateResetRuntimeRegistry();
  StateResetCommitWindow();

  g_state_ready = false;
  g_state_ready = HxNvsFormat(HX_NVS_STORE_STATE);
  if (!g_state_ready) {
    return false;
  }

  HX_LOGI("STA", "state storage formatted");
  return true;
}

bool StateGetStorageInfo(HxStateStorageInfo* out_info) {
  if (!out_info || !g_state_ready) {
    return false;
  }

  memset(out_info, 0, sizeof(*out_info));

  HxNvsStats stats{};
  if (!HxNvsGetStats(HX_NVS_STORE_STATE, &stats)) {
    return false;
  }

  size_t runtime_count = 0;
  size_t pending_count = 0;

  taskENTER_CRITICAL(&g_state_registry_mux);
  runtime_count = StateRuntimeCountLocked();
  taskEXIT_CRITICAL(&g_state_registry_mux);

  taskENTER_CRITICAL(&g_state_pending_mux);
  pending_count = StatePendingCountLocked();
  taskEXIT_CRITICAL(&g_state_pending_mux);

  out_info->ready = g_state_ready;
  out_info->partition_label = stats.partition_label;
  out_info->namespace_name = stats.namespace_name;
  out_info->commit_delay_ms = StateGetCommitDelayMs();
  out_info->entry_size_bytes = HX_STATE_NVS_ENTRY_SIZE;
  out_info->static_key_count = HX_STATIC_STATE_COUNT;
  out_info->runtime_key_count = runtime_count;
  out_info->total_key_count = HX_STATIC_STATE_COUNT + runtime_count;
  out_info->runtime_capacity = HX_STATE_RUNTIME_MAX;
  out_info->pending_key_count = pending_count;
  out_info->pending_capacity = HX_STATE_PENDING_MAX;
  out_info->partition_entries_used = stats.used_entries;
  out_info->partition_entries_free = stats.free_entries;
  out_info->partition_entries_available = stats.available_entries;
  out_info->partition_entries_total = stats.total_entries;
  out_info->namespace_entries_used = stats.namespace_entries;
  return true;
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

  bool handled = false;
  if (StateReadPendingBool(storage_key, &handled, value)) {
    return true;
  }

  if (handled) {
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

  bool handled = false;
  if (StateReadPendingInt(storage_key, &handled, value)) {
    if ((*value < def->min_i32) || (*value > def->max_i32)) {
      return false;
    }
    return true;
  }

  if (handled) {
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

  bool handled = false;
  if (StateReadPendingString(storage_key, &handled, out, out_size, def->max_len)) {
    return true;
  }

  if (handled) {
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

static bool StateSetBoolEx(const char* key, bool value, HxStateWriteSource source) {
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

  if (!StateIsWriteAllowed(def, source)) {
    return false;
  }

  if (!StateStagePendingBoolValue(storage_key, value)) {
    return false;
  }

  HX_LOGD("STA", "stage bool key=%s value=%s", def->key, value ? "true" : "false");
  return StateScheduleCommit();
}

static bool StateSetIntEx(const char* key, int32_t value, HxStateWriteSource source) {
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

  if (!StateIsWriteAllowed(def, source)) {
    return false;
  }

  if ((value < def->min_i32) || (value > def->max_i32)) {
    return false;
  }

  if (!StateStagePendingIntValue(storage_key, value)) {
    return false;
  }

  HX_LOGD("STA", "stage int key=%s value=%ld", def->key, (long)value);
  return StateScheduleCommit();
}

static bool StateSetStringEx(const char* key, const char* value, HxStateWriteSource source) {
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

  if (!StateIsWriteAllowed(def, source)) {
    return false;
  }

  size_t len = strlen(value);
  if (len > def->max_len) {
    return false;
  }

  if (!StateStagePendingStringValue(storage_key, value)) {
    return false;
  }

  HX_LOGD("STA", "stage string key=%s len=%lu", def->key, (unsigned long)len);
  return StateScheduleCommit();
}

bool StateErase(const char* key) {
  return StateEraseEx(key, HX_STATE_WRITE_SOURCE_USER);
}

bool StateSetValueFromString(const HxStateKeyDef* item, const char* value) {
  return StateSetValueFromStringEx(item, value, HX_STATE_WRITE_SOURCE_USER);
}

bool StateSetBool(const char* key, bool value) {
  return StateSetBoolEx(key, value, HX_STATE_WRITE_SOURCE_USER);
}

bool StateSetInt(const char* key, int32_t value) {
  return StateSetIntEx(key, value, HX_STATE_WRITE_SOURCE_USER);
}

bool StateSetString(const char* key, const char* value) {
  return StateSetStringEx(key, value, HX_STATE_WRITE_SOURCE_USER);
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
  if (!StateSetIntEx(key, next, HX_STATE_WRITE_SOURCE_USER)) {
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
  if (!StateSetIntEx(key, next, HX_STATE_WRITE_SOURCE_USER)) {
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
  if (!StateSetBoolEx(key, next, HX_STATE_WRITE_SOURCE_USER)) {
    return false;
  }

  if (new_value_out) {
    *new_value_out = next;
  }

  return true;
}