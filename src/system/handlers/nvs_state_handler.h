#pragma once

#include <stddef.h>
#include <stdint.h>

#include "headers/hx_config.h"


enum HxStateFlags : uint16_t {
  HX_STATE_FLAG_NONE            = 0,
  HX_STATE_FLAG_PERSISTENT      = 1 << 0,
  HX_STATE_FLAG_CONSOLE_VISIBLE = 1 << 1,
  HX_STATE_FLAG_API_VISIBLE     = 1 << 2,
  HX_STATE_FLAG_WRITE_RESTRICTED = 1 << 3,
  HX_STATE_FLAG_RUNTIME         = 1 << 4
};

enum HxStateWriteSource : uint8_t {
  HX_STATE_WRITE_SOURCE_USER = 0,
  HX_STATE_WRITE_SOURCE_SYSTEM = 1
};

enum HxStateOwnerClass : uint8_t {
  HX_STATE_OWNER_SYSTEM = 0,
  HX_STATE_OWNER_KERNEL = 1,
  HX_STATE_OWNER_USER = 2,
  HX_STATE_OWNER_INTERNAL = 3,
  HX_STATE_OWNER_EXTERNAL = 4
};

struct HxStateStorageInfo {
  bool ready;
  const char* partition_label;
  const char* namespace_name;
  uint32_t commit_delay_ms;
  size_t entry_size_bytes;
  size_t static_key_count;
  size_t runtime_key_count;
  size_t total_key_count;
  size_t runtime_capacity;
  size_t pending_key_count;
  size_t pending_capacity;
  size_t partition_entries_used;
  size_t partition_entries_free;
  size_t partition_entries_available;
  size_t partition_entries_total;
  size_t namespace_entries_used;
};

struct HxStateKeyDef {
  const char* key;
  HxSchemaValueType type;
  int32_t min_i32;
  int32_t max_i32;
  float min_f32;
  float max_f32;
  size_t max_len;
  uint16_t flags;
  HxStateOwnerClass owner_class;
};

size_t StateKeyCount();
const HxStateKeyDef* StateKeyAt(size_t index);
const HxStateKeyDef* StateFindKey(const char* key);

bool StateRegister(const HxStateKeyDef* def);
bool StateUnregister(const char* key);

bool StateCreate(const char* key,
                 HxSchemaValueType type,
                 int32_t min_i32,
                 int32_t max_i32,
                 float min_f32,
                 float max_f32,
                 size_t max_len,
                 uint16_t flags,
                 HxStateOwnerClass owner_class);

bool StateEnsure(const char* key,
                 HxSchemaValueType type,
                 int32_t min_i32,
                 int32_t max_i32,
                 float min_f32,
                 float max_f32,
                 size_t max_len,
                 uint16_t flags,
                 HxStateOwnerClass owner_class);

bool StateDelete(const char* key);

bool StateExists(const char* key);
bool StateErase(const char* key);

bool StateValueToString(const HxStateKeyDef* item, char* out, size_t out_size);
bool StateSetValueFromString(const HxStateKeyDef* item, const char* value);
bool StateWriteFromString(const char* key, const char* value);

bool StateReadBool(const char* key, bool* value);
bool StateReadInt(const char* key, int32_t* value);
bool StateReadFloat(const char* key, float* value);
bool StateReadString(const char* key, char* out, size_t out_size);

bool StateGetBoolOr(const char* key, bool defval);
int32_t StateGetIntOr(const char* key, int32_t defval);
float StateGetFloatOr(const char* key, float defval);
bool StateGetStringOr(const char* key, char* out, size_t out_size, const char* defval);

bool StateSetBool(const char* key, bool value);
bool StateSetInt(const char* key, int32_t value);
bool StateSetFloat(const char* key, float value);
bool StateSetString(const char* key, const char* value);

bool StateIncrementInt(const char* key, int32_t* new_value_out);
bool StateDecrementInt(const char* key, int32_t* new_value_out);
bool StateToggleBool(const char* key, bool* new_value_out);


// STATE HANDLER
bool StateInit();
bool StateLoad();
bool StateCommit();
bool StateFormat();
bool StateGetStorageInfo(HxStateStorageInfo* out_info);
void StateLoop();



#define HX_STATE_KEY_DECLARE_XS(id, key_text, max_len_value, console_visible_value, write_restricted_value) \
  static constexpr const char* HX_STATE_##id = key_text;

#define HX_STATE_KEY_DECLARE_XI(id, key_text, min_i32_value, max_i32_value, console_visible_value, write_restricted_value) \
  static constexpr const char* HX_STATE_##id = key_text;

#define HX_STATE_KEY_DECLARE_XB(id, key_text, console_visible_value, write_restricted_value) \
  static constexpr const char* HX_STATE_##id = key_text;

#define HX_STATE_KEY_DECLARE_XF(id, key_text, min_f32_value, max_f32_value, console_visible_value, write_restricted_value) \
  static constexpr const char* HX_STATE_##id = key_text;

HX_STATE_SCHEMA(HX_STATE_KEY_DECLARE_XS, HX_STATE_KEY_DECLARE_XI, HX_STATE_KEY_DECLARE_XB, HX_STATE_KEY_DECLARE_XF)

#undef HX_STATE_KEY_DECLARE_XF
#undef HX_STATE_KEY_DECLARE_XB
#undef HX_STATE_KEY_DECLARE_XI
#undef HX_STATE_KEY_DECLARE_XS
