/*
  HexaOS - command_builtin.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Built-in core commands for HexaOS.
  Registers the default service and diagnostics commands exposed by the shared
  command engine.
*/

#include "command_builtin.h"

#include <esp32-hal.h>
#include <esp_system.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers/hx_config.h"
#include "system/commands/command_engine.h"
#include "system/core/log.h"
#include "system/core/runtime.h"
#include "system/core/time.h"
#include "system/handlers/nvs_config_handler.h"
#include "system/handlers/nvs_state_handler.h"

static void CmdFormatFloatDisplay(char* out, size_t out_size, float value) {
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

static void CmdFormatUint64(char* out, size_t out_size, uint64_t value) {
  if (!TimeFormatUint64(out, out_size, value)) {
    if (out && (out_size > 0)) {
      out[0] = '\0';
    }
  }
}

static const char* CmdStateTypeText(HxSchemaValueType type) {
  switch (type) {
    case HX_SCHEMA_VALUE_BOOL:   return "bool";
    case HX_SCHEMA_VALUE_INT32:  return "int";
    case HX_SCHEMA_VALUE_STRING: return "string";
    case HX_SCHEMA_VALUE_FLOAT:  return "float";
    default:                     return "unknown";
  }
}

static const char* CmdStateScopeText(const HxStateKeyDef* item) {
  if (!item) {
    return "unknown";
  }

  return (item->flags & HX_STATE_FLAG_RUNTIME) ? "runtime" : "static";
}

static const char* CmdStateOwnerText(const HxStateKeyDef* item) {
  if (!item) {
    return "unknown";
  }

  switch (item->owner_class) {
    case HX_STATE_OWNER_SYSTEM:   return "system";
    case HX_STATE_OWNER_KERNEL:   return "kernel";
    case HX_STATE_OWNER_USER:     return "user";
    case HX_STATE_OWNER_INTERNAL: return "internal";
    case HX_STATE_OWNER_EXTERNAL: return "external";
    default:                      return "unknown";
  }
}

static const char* CmdSkipWs(const char* text) {
  if (!text) {
    return "";
  }

  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }

  return text;
}

static bool CmdExtractToken(const char** text_io, char* out, size_t out_size) {
  if (!text_io || !out || (out_size == 0)) {
    return false;
  }

  const char* text = CmdSkipWs(*text_io);
  if (!text[0]) {
    return false;
  }

  const char* end = text;
  while (*end && (*end != ' ') && (*end != '\t')) {
    end++;
  }

  size_t len = (size_t)(end - text);
  if ((len == 0) || (len >= out_size)) {
    return false;
  }

  memcpy(out, text, len);
  out[len] = '\0';

  *text_io = CmdSkipWs(end);
  return true;
}

static bool CmdParseInt32Token(const char** text_io, int32_t* value_out) {
  char token[32];

  if (!CmdExtractToken(text_io, token, sizeof(token))) {
    return false;
  }

  char* endptr = nullptr;
  long value = strtol(token, &endptr, 10);
  if ((endptr == token) || (*endptr != '\0')) {
    return false;
  }

  if ((value < INT32_MIN) || (value > INT32_MAX)) {
    return false;
  }

  *value_out = (int32_t)value;
  return true;
}

static bool CmdParseFloatToken(const char** text_io, float* value_out) {
  char token[32];

  if (!CmdExtractToken(text_io, token, sizeof(token))) {
    return false;
  }

  errno = 0;
  char* endptr = nullptr;
  float value = strtof(token, &endptr);
  if ((errno != 0) || (endptr == token) || (*endptr != '\0') || !isfinite(value)) {
    return false;
  }

  *value_out = value;
  return true;
}

static void CmdWriteConfigItemLine(const HxConfigKeyDef* item, HxCmdOutput* out) {
  if (!item) {
    return;
  }

  char current[96];
  char defaults[96];

  if (!ConfigValueToString(item, current, sizeof(current))) {
    snprintf(current, sizeof(current), "<error>");
  }

  if (!ConfigDefaultToString(item, defaults, sizeof(defaults))) {
    snprintf(defaults, sizeof(defaults), "<error>");
  }

  CmdOutPrintfLine(out, "%s = %s (default=%s)", item->key, current, defaults);
}

static void CmdWriteStateItemLine(const HxStateKeyDef* item, HxCmdOutput* out) {
  if (!item) {
    return;
  }

  char value[160];

  if (!StateValueToString(item, value, sizeof(value))) {
    snprintf(value, sizeof(value), "<unset>");
  }

  CmdOutPrintfLine(out,
                   "%s [%s,%s,owner=%s] = %s",
                   item->key,
                   CmdStateTypeText(item->type),
                   CmdStateScopeText(item),
                   CmdStateOwnerText(item),
                   value);
}

static bool CmdSplitKeyValue(const char* text, char* key_out, size_t key_size, const char** value_out) {
  if (!text || !key_out || (key_size == 0) || !value_out) {
    return false;
  }

  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }

  if (!text[0]) {
    return false;
  }

  const char* sep = text;
  while (*sep && (*sep != ' ') && (*sep != '\t')) {
    sep++;
  }

  size_t key_len = (size_t)(sep - text);
  if ((key_len == 0) || (key_len >= key_size)) {
    return false;
  }

  memcpy(key_out, text, key_len);
  key_out[key_len] = '\0';

  while ((*sep == ' ') || (*sep == '\t')) {
    sep++;
  }

  *value_out = sep;
  return (sep[0] != '\0');
}

static bool CmdExtractSingleKey(const char* text, char* key_out, size_t key_size) {
  if (!text || !key_out || (key_size == 0)) {
    return false;
  }

  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }

  if (!text[0]) {
    return false;
  }

  const char* end = text;
  while (*end && (*end != ' ') && (*end != '\t')) {
    end++;
  }

  while ((*end == ' ') || (*end == '\t')) {
    end++;
  }

  if (*end != '\0') {
    return false;
  }

  size_t len = (size_t)(end - text);
  while ((len > 0) && ((text[len - 1] == ' ') || (text[len - 1] == '\t'))) {
    len--;
  }

  if ((len == 0) || (len >= key_size)) {
    return false;
  }

  memcpy(key_out, text, len);
  key_out[len] = '\0';
  return true;
}

static HxCmdStatus CmdHelp(const char* args, HxCmdOutput* out) {
  (void)args;

  CmdOutWriteLine(out, "commands:");

  for (size_t i = 0; i < CommandCount(); i++) {
    const HxCmdDef* def = CommandAt(i);
    if (!def || !def->name || !def->help || !def->help[0]) {
      continue;
    }

    CmdOutPrintfLine(out, "  %s - %s", def->name, def->help);
  }

  return HX_CMD_OK;
}

static HxCmdStatus CmdReboot(const char* args, HxCmdOutput* out) {
  (void)args;
  (void)out;

  LogWarn("CMD: soft restart requested");
  delay(100);
  esp_restart();
  return HX_CMD_OK;
}

static HxCmdStatus CmdLogHistory(const char* args, HxCmdOutput* out) {
  (void)args;

  size_t used = LogHistorySize();
  if (used == 0) {
    CmdOutWriteLine(out, "log history is empty");
    return HX_CMD_OK;
  }

  char* dump = (char*)malloc(used + 1);
  if (!dump) {
    CmdOutWriteLine(out, "log history dump failed: out of memory");
    return HX_CMD_ERROR;
  }

  size_t copied = LogHistoryCopy(dump, used + 1);
  if (copied > 0) {
    CmdOutWriteRaw(out, dump);
  }

  free(dump);
  return HX_CMD_OK;
}

static HxCmdStatus CmdLogClear(const char* args, HxCmdOutput* out) {
  (void)args;

  LogHistoryClear();
  CmdOutWriteLine(out, "log history cleared");
  return HX_CMD_OK;
}

static HxCmdStatus CmdLogStats(const char* args, HxCmdOutput* out) {
  (void)args;

  CmdOutPrintfLine(out,
                   "log: used=%lu capacity=%lu dropped_lines=%lu dropped_isr=%lu",
                   (unsigned long)LogHistorySize(),
                   (unsigned long)LogHistoryCapacity(),
                   (unsigned long)LogDroppedLines(),
                   (unsigned long)LogDroppedIsr());
  return HX_CMD_OK;
}

static HxCmdStatus CmdListConfig(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: config list");
    return HX_CMD_USAGE;
  }

  for (size_t i = 0; i < ConfigKeyCount(); i++) {
    const HxConfigKeyDef* item = ConfigKeyAt(i);
    if (!item || !item->console_visible) {
      continue;
    }

    CmdWriteConfigItemLine(item, out);
  }

  CmdOutPrintfLine(out, "config.loaded = %s", Hx.config_loaded ? "true" : "false");
  return HX_CMD_OK;
}

static HxCmdStatus CmdReadConfig(const char* args, HxCmdOutput* out) {
  char key[64];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: config read <key>");
    return HX_CMD_USAGE;
  }

  const HxConfigKeyDef* item = ConfigFindConfigKey(key);
  if (!item) {
    CmdOutWriteLine(out, "config key not found");
    return HX_CMD_ERROR;
  }

  CmdWriteConfigItemLine(item, out);
  return HX_CMD_OK;
}

static HxCmdStatus CmdSetConfig(const char* args, HxCmdOutput* out) {
  char key[64];
  const char* value = nullptr;

  if (!CmdSplitKeyValue(args, key, sizeof(key), &value)) {
    CmdOutWriteLine(out, "usage: config set <key> <value>");
    return HX_CMD_USAGE;
  }

  const HxConfigKeyDef* item = ConfigFindConfigKey(key);
  if (!item) {
    CmdOutWriteLine(out, "config key not found");
    return HX_CMD_ERROR;
  }

  if (!item->console_writable) {
    CmdOutWriteLine(out, "config key is read-only");
    return HX_CMD_ERROR;
  }

  if (!ConfigSetValueFromString(item, value)) {
    CmdOutWriteLine(out, "invalid config value");
    return HX_CMD_ERROR;
  }

  ConfigApply();
  CmdWriteConfigItemLine(item, out);
  return HX_CMD_OK;
}

static HxCmdStatus CmdToggleConfig(const char* args, HxCmdOutput* out) {
  char key[64];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: config toggle <key>");
    return HX_CMD_USAGE;
  }

  const HxConfigKeyDef* item = ConfigFindConfigKey(key);
  if (!item) {
    CmdOutWriteLine(out, "config key not found");
    return HX_CMD_ERROR;
  }

  if (item->type != HX_SCHEMA_VALUE_BOOL) {
    CmdOutWriteLine(out, "config key is not boolean");
    return HX_CMD_ERROR;
  }

  if (!item->console_writable) {
    CmdOutWriteLine(out, "config key is read-only");
    return HX_CMD_ERROR;
  }

  bool next = false;
  if (!ConfigToggleBool(item, &next)) {
    CmdOutWriteLine(out, "config toggle failed");
    return HX_CMD_ERROR;
  }

  ConfigApply();
  CmdOutPrintfLine(out, "%s = %s", item->key, next ? "true" : "false");
  return HX_CMD_OK;
}

static HxCmdStatus CmdSaveConfig(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: config save");
    return HX_CMD_USAGE;
  }

  if (ConfigSave()) {
    CmdOutWriteLine(out, "config saved to NVS");
    return HX_CMD_OK;
  }

  CmdOutWriteLine(out, "config save failed");
  return HX_CMD_ERROR;
}

static HxCmdStatus CmdLoadConfig(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: config load");
    return HX_CMD_USAGE;
  }

  if (ConfigLoad()) {
    ConfigApply();
    CmdOutWriteLine(out, "config loaded from NVS");
    return HX_CMD_OK;
  }

  CmdOutWriteLine(out, "config load failed");
  return HX_CMD_ERROR;
}

static HxCmdStatus CmdDefaultConfig(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: config factorydefault");
    return HX_CMD_USAGE;
  }

  ConfigResetToDefaults(&HxConfigData);
  ConfigApply();
  CmdOutWriteLine(out, "config reset to build defaults");
  return HX_CMD_OK;
}

static HxCmdStatus CmdConfigInfo(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: config info");
    return HX_CMD_USAGE;
  }

  HxConfigStorageInfo info{};
  if (!ConfigGetStorageInfo(&info)) {
    CmdOutWriteLine(out, "config info failed");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "partition = %s", info.partition_label ? info.partition_label : "-");
  CmdOutPrintfLine(out, "namespace = %s", info.namespace_name ? info.namespace_name : "-");
  CmdOutPrintfLine(out, "ready = %s", info.ready ? "true" : "false");
  CmdOutPrintfLine(out, "loaded = %s", info.loaded ? "true" : "false");
  CmdOutPrintfLine(out,
                   "partition.entries = used=%lu free=%lu available=%lu total=%lu",
                   (unsigned long)info.partition_entries_used,
                   (unsigned long)info.partition_entries_free,
                   (unsigned long)info.partition_entries_available,
                   (unsigned long)info.partition_entries_total);
  CmdOutPrintfLine(out,
                   "partition.bytes_approx = used=%lu free=%lu total=%lu entry_size=%lu",
                   (unsigned long)(info.partition_entries_used * info.entry_size_bytes),
                   (unsigned long)(info.partition_entries_free * info.entry_size_bytes),
                   (unsigned long)(info.partition_entries_total * info.entry_size_bytes),
                   (unsigned long)info.entry_size_bytes);
  CmdOutPrintfLine(out, "namespace.entries = %lu", (unsigned long)info.namespace_entries_used);
  CmdOutPrintfLine(out,
                   "keys = total=%lu visible=%lu writable=%lu overridden=%lu",
                   (unsigned long)info.total_key_count,
                   (unsigned long)info.visible_key_count,
                   (unsigned long)info.writable_key_count,
                   (unsigned long)info.overridden_key_count);
  return HX_CMD_OK;
}

static HxCmdStatus CmdConfigFactoryFormat(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: config factoryformat");
    return HX_CMD_USAGE;
  }

  if (!ConfigFactoryFormat()) {
    CmdOutWriteLine(out, "config factory format failed");
    return HX_CMD_ERROR;
  }

  ConfigApply();
  CmdOutWriteLine(out, "config storage formatted and defaults activated");
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateList(const char* args, HxCmdOutput* out) {
  const char* prefix = CmdSkipWs(args);
  size_t prefix_len = 0;

  if (prefix[0]) {
    const char* end = prefix;
    while (*end && (*end != ' ') && (*end != '\t')) {
      end++;
    }

    if (*end != '\0') {
      CmdOutWriteLine(out, "usage: state list [prefix]");
      return HX_CMD_USAGE;
    }

    prefix_len = strlen(prefix);
  }

  size_t shown = 0;

  for (size_t i = 0; i < StateKeyCount(); i++) {
    const HxStateKeyDef* item = StateKeyAt(i);
    if (!item) {
      continue;
    }

    if ((item->flags & HX_STATE_FLAG_CONSOLE_VISIBLE) == 0) {
      continue;
    }

    if (prefix_len > 0) {
      if (strncmp(item->key, prefix, prefix_len) != 0) {
        continue;
      }
    }

    CmdWriteStateItemLine(item, out);
    shown++;
  }

  if (shown == 0) {
    CmdOutWriteLine(out, "no matching states");
  }

  return HX_CMD_OK;
}

static HxCmdStatus CmdStateRead(const char* args, HxCmdOutput* out) {
  char key[96];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: state read <key>");
    return HX_CMD_USAGE;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    CmdOutWriteLine(out, "state key not found");
    return HX_CMD_ERROR;
  }

  CmdWriteStateItemLine(item, out);
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateExist(const char* args, HxCmdOutput* out) {
  char key[96];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: state exist <key>");
    return HX_CMD_USAGE;
  }

  CmdOutWriteLine(out, StateExists(key) ? "true" : "false");
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateCreate(const char* args, HxCmdOutput* out) {
  const char* text = args;
  char key[96];
  char type[16];

  if (!CmdExtractToken(&text, key, sizeof(key)) || !CmdExtractToken(&text, type, sizeof(type))) {
    CmdOutWriteLine(out, "usage:");
    CmdOutWriteLine(out, "  state create <key> bool");
    CmdOutWriteLine(out, "  state create <key> int <min> <max>");
    CmdOutWriteLine(out, "  state create <key> float <min> <max>");
    CmdOutWriteLine(out, "  state create <key> string <max_len>");
    return HX_CMD_USAGE;
  }

  if (StateExists(key)) {
    CmdOutWriteLine(out, "state already exists");
    return HX_CMD_ERROR;
  }

  if (strcmp(type, "bool") == 0) {
    if (CmdSkipWs(text)[0] != '\0') {
      CmdOutWriteLine(out, "usage: state create <key> bool");
      return HX_CMD_USAGE;
    }

    if (!StateCreate(key,
                     HX_SCHEMA_VALUE_BOOL,
                     0,
                     1,
                     0.0f,
                     0.0f,
                     0,
                     HX_STATE_FLAG_CONSOLE_VISIBLE,
                     HX_STATE_OWNER_USER)) {
      CmdOutWriteLine(out, "state create failed");
      return HX_CMD_ERROR;
    }

    CmdOutPrintfLine(out, "created bool state: %s", key);
    return HX_CMD_OK;
  }

  if ((strcmp(type, "int") == 0) || (strcmp(type, "int32") == 0)) {
    int32_t min_i32 = 0;
    int32_t max_i32 = 0;

    if (!CmdParseInt32Token(&text, &min_i32) || !CmdParseInt32Token(&text, &max_i32) || (CmdSkipWs(text)[0] != '\0')) {
      CmdOutWriteLine(out, "usage: state create <key> int <min> <max>");
      return HX_CMD_USAGE;
    }

    if (min_i32 > max_i32) {
      CmdOutWriteLine(out, "invalid integer range");
      return HX_CMD_ERROR;
    }

    if (!StateCreate(key,
                     HX_SCHEMA_VALUE_INT32,
                     min_i32,
                     max_i32,
                     0.0f,
                     0.0f,
                     0,
                     HX_STATE_FLAG_CONSOLE_VISIBLE,
                     HX_STATE_OWNER_USER)) {
      CmdOutWriteLine(out, "state create failed");
      return HX_CMD_ERROR;
    }

    CmdOutPrintfLine(out, "created int state: %s [%ld..%ld]", key, (long)min_i32, (long)max_i32);
    return HX_CMD_OK;
  }


  if ((strcmp(type, "float") == 0) || (strcmp(type, "f32") == 0)) {
    float min_f32 = 0.0f;
    float max_f32 = 0.0f;

    if (!CmdParseFloatToken(&text, &min_f32) || !CmdParseFloatToken(&text, &max_f32) || (CmdSkipWs(text)[0] != '\0')) {
      CmdOutWriteLine(out, "usage: state create <key> float <min> <max>");
      return HX_CMD_USAGE;
    }

    if (min_f32 > max_f32) {
      CmdOutWriteLine(out, "invalid float range");
      return HX_CMD_ERROR;
    }

    if (!StateCreate(key,
                     HX_SCHEMA_VALUE_FLOAT,
                     0,
                     0,
                     min_f32,
                     max_f32,
                     0,
                     HX_STATE_FLAG_CONSOLE_VISIBLE,
                     HX_STATE_OWNER_USER)) {
      CmdOutWriteLine(out, "state create failed");
      return HX_CMD_ERROR;
    }

    char min_text[32];
    char max_text[32];
    CmdFormatFloatDisplay(min_text, sizeof(min_text), min_f32);
    CmdFormatFloatDisplay(max_text, sizeof(max_text), max_f32);
    CmdOutPrintfLine(out, "created float state: %s [%s..%s]", key, min_text, max_text);
    return HX_CMD_OK;
  }

  if (strcmp(type, "string") == 0) {
    int32_t max_len = 0;

    if (!CmdParseInt32Token(&text, &max_len) || (CmdSkipWs(text)[0] != '\0')) {
      CmdOutWriteLine(out, "usage: state create <key> string <max_len>");
      return HX_CMD_USAGE;
    }

    if (max_len <= 0) {
      CmdOutWriteLine(out, "invalid string max_len");
      return HX_CMD_ERROR;
    }

    if (!StateCreate(key,
                     HX_SCHEMA_VALUE_STRING,
                     0,
                     0,
                     0.0f,
                     0.0f,
                     (size_t)max_len,
                     HX_STATE_FLAG_CONSOLE_VISIBLE,
                     HX_STATE_OWNER_USER)) {
      CmdOutWriteLine(out, "state create failed");
      return HX_CMD_ERROR;
    }

    CmdOutPrintfLine(out, "created string state: %s [max_len=%ld]", key, (long)max_len);
    return HX_CMD_OK;
  }

  CmdOutWriteLine(out, "unsupported state type");
  return HX_CMD_ERROR;
}

static HxCmdStatus CmdStateWrite(const char* args, HxCmdOutput* out) {
  char key[96];
  const char* value = nullptr;

  if (!CmdSplitKeyValue(args, key, sizeof(key), &value)) {
    CmdOutWriteLine(out, "usage: state write <key> <value>");
    return HX_CMD_USAGE;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    CmdOutWriteLine(out, "state key not found");
    return HX_CMD_ERROR;
  }

  if (!StateWriteFromString(key, value)) {
    CmdOutWriteLine(out, "state write failed");
    return HX_CMD_ERROR;
  }

  CmdWriteStateItemLine(item, out);
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateErase(const char* args, HxCmdOutput* out) {
  char key[96];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: state erase <key>");
    return HX_CMD_USAGE;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    CmdOutWriteLine(out, "state key not found");
    return HX_CMD_ERROR;
  }

  if (!StateErase(key)) {
    CmdOutWriteLine(out, "state erase failed");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "%s erased", key);
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateDelete(const char* args, HxCmdOutput* out) {
  char key[96];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: state delete <key>");
    return HX_CMD_USAGE;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    CmdOutWriteLine(out, "state key not found");
    return HX_CMD_ERROR;
  }

  if ((item->flags & HX_STATE_FLAG_RUNTIME) == 0) {
    CmdOutWriteLine(out, "state is not runtime");
    return HX_CMD_ERROR;
  }

  if (!StateDelete(key)) {
    CmdOutWriteLine(out, "state delete failed");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "%s deleted", key);
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateIncrement(const char* args, HxCmdOutput* out) {
  char key[96];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: state increment <key>");
    return HX_CMD_USAGE;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    CmdOutWriteLine(out, "state key not found");
    return HX_CMD_ERROR;
  }

  if (item->type != HX_SCHEMA_VALUE_INT32) {
    CmdOutWriteLine(out, "state is not integer");
    return HX_CMD_ERROR;
  }

  int32_t next = 0;
  if (!StateIncrementInt(key, &next)) {
    CmdOutWriteLine(out, "state increment failed");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "%s = %ld", key, (long)next);
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateDecrement(const char* args, HxCmdOutput* out) {
  char key[96];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: state decrement <key>");
    return HX_CMD_USAGE;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    CmdOutWriteLine(out, "state key not found");
    return HX_CMD_ERROR;
  }

  if (item->type != HX_SCHEMA_VALUE_INT32) {
    CmdOutWriteLine(out, "state is not integer");
    return HX_CMD_ERROR;
  }

  int32_t next = 0;
  if (!StateDecrementInt(key, &next)) {
    CmdOutWriteLine(out, "state decrement failed");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "%s = %ld", key, (long)next);
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateToggle(const char* args, HxCmdOutput* out) {
  char key[96];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: state toggle <key>");
    return HX_CMD_USAGE;
  }

  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    CmdOutWriteLine(out, "state key not found");
    return HX_CMD_ERROR;
  }

  if (item->type != HX_SCHEMA_VALUE_BOOL) {
    CmdOutWriteLine(out, "state is not boolean");
    return HX_CMD_ERROR;
  }

  bool next = false;
  if (!StateToggleBool(key, &next)) {
    CmdOutWriteLine(out, "state toggle failed");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "%s = %s", key, next ? "true" : "false");
  return HX_CMD_OK;
}


static HxCmdStatus CmdStateInfo(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: state info");
    return HX_CMD_USAGE;
  }

  HxStateStorageInfo info{};
  if (!StateGetStorageInfo(&info)) {
    CmdOutWriteLine(out, "state info failed");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "partition = %s", info.partition_label ? info.partition_label : "-");
  CmdOutPrintfLine(out, "namespace = %s", info.namespace_name ? info.namespace_name : "-");
  CmdOutPrintfLine(out, "ready = %s", info.ready ? "true" : "false");
  CmdOutPrintfLine(out, "delay_ms = %lu", (unsigned long)info.commit_delay_ms);
  CmdOutPrintfLine(out,
                   "partition.entries = used=%lu free=%lu available=%lu total=%lu",
                   (unsigned long)info.partition_entries_used,
                   (unsigned long)info.partition_entries_free,
                   (unsigned long)info.partition_entries_available,
                   (unsigned long)info.partition_entries_total);
  CmdOutPrintfLine(out,
                   "partition.bytes_approx = used=%lu free=%lu total=%lu entry_size=%lu",
                   (unsigned long)(info.partition_entries_used * info.entry_size_bytes),
                   (unsigned long)(info.partition_entries_free * info.entry_size_bytes),
                   (unsigned long)(info.partition_entries_total * info.entry_size_bytes),
                   (unsigned long)info.entry_size_bytes);
  CmdOutPrintfLine(out, "namespace.entries = %lu", (unsigned long)info.namespace_entries_used);
  CmdOutPrintfLine(out,
                   "keys = static=%lu runtime=%lu total=%lu runtime_capacity=%lu",
                   (unsigned long)info.static_key_count,
                   (unsigned long)info.runtime_key_count,
                   (unsigned long)info.total_key_count,
                   (unsigned long)info.runtime_capacity);
  CmdOutPrintfLine(out,
                   "pending = used=%lu capacity=%lu",
                   (unsigned long)info.pending_key_count,
                   (unsigned long)info.pending_capacity);
  return HX_CMD_OK;
}

static HxCmdStatus CmdStateFormat(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: state format");
    return HX_CMD_USAGE;
  }

  if (!StateFormat()) {
    CmdOutWriteLine(out, "state format failed");
    return HX_CMD_ERROR;
  }

  CmdOutWriteLine(out, "state storage formatted");
  return HX_CMD_OK;
}


static HxCmdStatus CmdTimeStatus(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: time status");
    return HX_CMD_USAGE;
  }

  HxTimeInfo info{};
  if (!TimeGetInfo(&info)) {
    CmdOutWriteLine(out, "time status failed");
    return HX_CMD_ERROR;
  }

  char monotonic_text[32];
  TimeFormatMonotonic(monotonic_text, sizeof(monotonic_text), info.monotonic_ms);

  CmdOutPrintfLine(out, "ready = %s", info.ready ? "true" : "false");
  CmdOutPrintfLine(out, "synchronized = %s", info.synchronized ? "true" : "false");
  CmdOutPrintfLine(out, "source = %s", TimeSourceText(info.source));
  char monotonic_ms_text[24];
  CmdFormatUint64(monotonic_ms_text, sizeof(monotonic_ms_text), info.monotonic_ms);
  CmdOutPrintfLine(out, "monotonic_ms = %s", monotonic_ms_text);
  CmdOutPrintfLine(out, "uptime = %s", monotonic_text);

  if (!info.synchronized) {
    CmdOutWriteLine(out, "utc = -");
    return HX_CMD_OK;
  }

  char utc_text[40];
  if (!TimeFormatUtc(utc_text, sizeof(utc_text), info.unix_ms)) {
    CmdOutWriteLine(out, "utc = <format-error>");
    return HX_CMD_OK;
  }

  char unix_ms_text[24];
  char unix_s_text[24];
  char sync_age_text[24];
  CmdFormatUint64(unix_ms_text, sizeof(unix_ms_text), info.unix_ms);
  CmdFormatUint64(unix_s_text, sizeof(unix_s_text), info.unix_ms / 1000ULL);
  CmdFormatUint64(sync_age_text, sizeof(sync_age_text), info.sync_age_ms);
  CmdOutPrintfLine(out, "unix_ms = %s", unix_ms_text);
  CmdOutPrintfLine(out, "unix_s = %s", unix_s_text);
  CmdOutPrintfLine(out, "sync_age_ms = %s", sync_age_text);
  CmdOutPrintfLine(out, "utc = %s", utc_text);
  return HX_CMD_OK;
}

static HxCmdStatus CmdTimeSetEpoch(const char* args, HxCmdOutput* out) {
  uint64_t unix_seconds = 0;

  const char* text = CmdSkipWs(args);
  if (text[0] == '\0') {
    CmdOutWriteLine(out, "usage: time setepoch <unix_seconds>");
    return HX_CMD_USAGE;
  }

  char token[32];
  if (!CmdExtractToken(&text, token, sizeof(token)) || (CmdSkipWs(text)[0] != '\0')) {
    CmdOutWriteLine(out, "usage: time setepoch <unix_seconds>");
    return HX_CMD_USAGE;
  }

  char* endptr = nullptr;
  unsigned long long parsed = strtoull(token, &endptr, 10);
  if ((endptr == token) || (*endptr != '\0')) {
    CmdOutWriteLine(out, "invalid unix_seconds");
    return HX_CMD_ERROR;
  }

  unix_seconds = (uint64_t)parsed;
  if (!TimeSetUnixSeconds(unix_seconds, HX_TIME_SOURCE_USER)) {
    CmdOutWriteLine(out, "time set failed");
    return HX_CMD_ERROR;
  }

  char utc_text[40];
  if (!TimeFormatNowUtc(utc_text, sizeof(utc_text))) {
    CmdOutWriteLine(out, "time set but formatting failed");
    return HX_CMD_OK;
  }

  CmdOutPrintfLine(out, "time set to %s", utc_text);
  return HX_CMD_OK;
}

static HxCmdStatus CmdTimeClear(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: time clear");
    return HX_CMD_USAGE;
  }

  TimeClearSynchronization();
  CmdOutWriteLine(out, "time synchronization cleared");
  return HX_CMD_OK;
}

static const HxCmdDef kBuiltinCommands[] = {
  { "help",                 CmdHelp,                "Show command list" },
  { "?",                    CmdHelp,                nullptr },
  { "reboot",               CmdReboot,              "Restart device" },
  { "log",                  CmdLogHistory,          "Show log history" },
  { "logclr",               CmdLogClear,            "Clear log history" },
  { "logstat",              CmdLogStats,            "Show log statistics" },
  { "time",                 CmdTimeStatus,          "Show current time status" },
  { "time status",          CmdTimeStatus,          nullptr },
  { "time setepoch",        CmdTimeSetEpoch,        "Set synchronized time from unix seconds" },
  { "time clear",           CmdTimeClear,           "Clear synchronized wall clock" },
  { "config list",          CmdListConfig,          "List visible config keys" },
  { "config read",          CmdReadConfig,          "Read config key" },
  { "config set",           CmdSetConfig,           "Set config key" },
  { "config save",          CmdSaveConfig,          "Save config to NVS" },
  { "config load",          CmdLoadConfig,          "Load config from NVS" },
  { "config default",       CmdDefaultConfig,       "Reset config to build defaults" },
  { "config toggle",        CmdToggleConfig,        "Toggle boolean config key" },
  { "config info",          CmdConfigInfo,          "Show config storage information" },
  { "config factoryformat", CmdConfigFactoryFormat, "Format config NVS storage and activate defaults" },
  { "state info",           CmdStateInfo,           "Show state storage information" },
  { "state format",         CmdStateFormat,         "Format state NVS storage" },
  { "state list",           CmdStateList,           "List visible states" },
  { "state read",           CmdStateRead,           "Read state key" },
  { "state exist",          CmdStateExist,          "Check whether state exists" },
  { "state create",         CmdStateCreate,         "Create persistent runtime state" },
  { "state write",          CmdStateWrite,          "Write value to state" },
  { "state erase",          CmdStateErase,          "Erase persisted state value" },
  { "state delete",         CmdStateDelete,         "Delete runtime state" },
  { "state unreg",          CmdStateDelete,         nullptr },
  { "state increment",      CmdStateIncrement,      "Increment integer state" },
  { "state decrement",      CmdStateDecrement,      "Decrement integer state" },
  { "state toggle",         CmdStateToggle,         "Toggle boolean state" }
};

bool CommandRegisterBuiltins() {
  for (size_t i = 0; i < (sizeof(kBuiltinCommands) / sizeof(kBuiltinCommands[0])); i++) {
    if (!CommandRegister(&kBuiltinCommands[i])) {
      return false;
    }
  }

  return true;
}