#pragma once

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

enum HxStateFlags : uint16_t {
  HX_STATE_FLAG_NONE            = 0,
  HX_STATE_FLAG_PERSISTENT      = 1 << 0,
  HX_STATE_FLAG_CONSOLE_VISIBLE = 1 << 1,
  HX_STATE_FLAG_API_VISIBLE     = 1 << 2,
  HX_STATE_FLAG_READONLY        = 1 << 3,
  HX_STATE_FLAG_RUNTIME         = 1 << 4
};

struct HxStateKeyDef {
  const char* key;
  HxSchemaValueType type;
  int32_t min_i32;
  int32_t max_i32;
  size_t max_len;
  uint16_t flags;
  bool console_visible;
  const char* owner;
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

bool StateRegister(const HxStateKeyDef* def);
bool StateUnregister(const char* key);

bool StateCreate(const char* key,
                 HxSchemaValueType type,
                 int32_t min_i32,
                 int32_t max_i32,
                 size_t max_len,
                 uint16_t flags,
                 bool console_visible,
                 const char* owner);

bool StateEnsure(const char* key,
                 HxSchemaValueType type,
                 int32_t min_i32,
                 int32_t max_i32,
                 size_t max_len,
                 uint16_t flags,
                 bool console_visible,
                 const char* owner);

bool StateDelete(const char* key);

bool StateExists(const char* key);
bool StateErase(const char* key);

bool StateValueToString(const HxStateKeyDef* item, char* out, size_t out_size);
bool StateSetValueFromString(const HxStateKeyDef* item, const char* value);
bool StateWriteFromString(const char* key, const char* value);

bool StateReadBool(const char* key, bool* value);
bool StateReadInt(const char* key, int32_t* value);
bool StateReadString(const char* key, char* out, size_t out_size);

bool StateGetBoolOr(const char* key, bool defval);
int32_t StateGetIntOr(const char* key, int32_t defval);
bool StateGetStringOr(const char* key, char* out, size_t out_size, const char* defval);

bool StateSetBool(const char* key, bool value);
bool StateSetInt(const char* key, int32_t value);
bool StateSetString(const char* key, const char* value);

bool StateIncrementInt(const char* key, int32_t* new_value_out);
bool StateDecrementInt(const char* key, int32_t* new_value_out);
bool StateToggleBool(const char* key, bool* new_value_out);

extern HxConfig HxConfigData;
extern const HxConfig HxConfigDefaults;

void ConfigResetToDefaults(HxConfig* config);

bool ConfigInit();
bool ConfigLoad();
bool ConfigSave();
void ConfigApply();

// STATE HANDLER
bool StateInit();
bool StateLoad();
bool StateSave();
bool StateCommit();
void StateLoop();

// FACTORY HANDLER
bool FactoryDataInit();

#define HX_CFG_KEY_DECLARE(id, key_text, type_id, field_name, storage_size, max_len_value, min_i32_value, max_i32_value, default_value, console_visible_value, console_writable_value) \
  static constexpr const char* HX_CFG_##id = key_text;

HX_CONFIG_SCHEMA(HX_CFG_KEY_DECLARE)

#undef HX_CFG_KEY_DECLARE

#define HX_STATE_KEY_DECLARE(id, key_text, type_id, min_i32, max_i32, max_len, console_visible) \
  static constexpr const char* HX_STATE_##id = key_text;

HX_STATE_SCHEMA(HX_STATE_KEY_DECLARE)

#undef HX_STATE_KEY_DECLARE