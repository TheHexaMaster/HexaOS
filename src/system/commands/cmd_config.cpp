/*
  HexaOS - cmd_config.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Configuration inspection and control commands for HexaOS.
  Registers: config list, config read, config set, config save, config load,
             config default, config toggle, config info, config factoryformat.
*/

#include <stdio.h>
#include <string.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "system/core/config.h"
#include "system/core/runtime.h"

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

static const HxCmdDef kConfigCommands[] = {
  { "config list",          CmdListConfig,          "List visible config keys" },
  { "config read",          CmdReadConfig,          "Read config key" },
  { "config set",           CmdSetConfig,           "Set config key" },
  { "config save",          CmdSaveConfig,          "Save config to storage" },
  { "config load",          CmdLoadConfig,          "Load config from storage" },
  { "config default",       CmdDefaultConfig,       "Reset config to build defaults" },
  { "config toggle",        CmdToggleConfig,        "Toggle boolean config key" },
  { "config info",          CmdConfigInfo,          "Show config storage information" },
  { "config factoryformat", CmdConfigFactoryFormat, "Format config storage and activate defaults" }
};

bool CommandRegisterConfig() {
  for (size_t i = 0; i < (sizeof(kConfigCommands) / sizeof(kConfigCommands[0])); i++) {
    if (!CommandRegister(&kConfigCommands[i])) {
      return false;
    }
  }
  return true;
}
