/*
  HexaOS - cmd_state.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  State registry inspection and control commands for HexaOS.
  Registers: state info, state format, state list, state read, state exist,
             state create, state write, state erase, state delete, state
             increment, state decrement, state toggle, state unreg.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "system/core/state.h"

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
                     0, 1, 0.0f, 0.0f, 0,
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
                     min_i32, max_i32, 0.0f, 0.0f, 0,
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
                     0, 0, min_f32, max_f32, 0,
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
                     0, 0, 0.0f, 0.0f, (size_t)max_len,
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

static const HxCmdDef kStateCommands[] = {
  { "state info",      CmdStateInfo,      "Show state storage information" },
  { "state format",    CmdStateFormat,    "Format state storage" },
  { "state list",      CmdStateList,      "List visible states" },
  { "state read",      CmdStateRead,      "Read state key" },
  { "state exist",     CmdStateExist,     "Check whether state exists" },
  { "state create",    CmdStateCreate,    "Create persistent runtime state" },
  { "state write",     CmdStateWrite,     "Write value to state" },
  { "state erase",     CmdStateErase,     "Erase persisted state value" },
  { "state delete",    CmdStateDelete,    "Delete runtime state" },
  { "state unreg",     CmdStateDelete,    nullptr },
  { "state increment", CmdStateIncrement, "Increment integer state" },
  { "state decrement", CmdStateDecrement, "Decrement integer state" },
  { "state toggle",    CmdStateToggle,    "Toggle boolean state" }
};

bool CommandRegisterState() {
  for (size_t i = 0; i < (sizeof(kStateCommands) / sizeof(kStateCommands[0])); i++) {
    if (!CommandRegister(&kStateCommands[i])) {
      return false;
    }
  }
  return true;
}
