/*
  HexaOS - command_builtin.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Built-in core commands for HexaOS.
  Registers the default service and diagnostics commands exposed by the shared
  command engine.
*/

#include "hexaos.h"
#include "system/commands/command_engine.h"
#include "system/commands/command_builtin.h"

#include <esp_system.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void CmdWriteConfigItemLine(const HxConfigKeyDef* item, HxCmdOutput* out) {
  if (!item) {
    return;
  }

  char current[96];
  char defaults[96];

  if (!ConfigConfigValueToString(item, current, sizeof(current))) {
    snprintf(current, sizeof(current), "<error>");
  }

  if (!ConfigConfigDefaultToString(item, defaults, sizeof(defaults))) {
    snprintf(defaults, sizeof(defaults), "<error>");
  }

  CmdOutPrintfLine(out, "%s = %s (default=%s)", item->key, current, defaults);
}

static void CmdWriteStateItemLine(const HxStateKeyDef* item, HxCmdOutput* out) {
  if (!item) {
    return;
  }

  char value[96];

  if (StateValueToString(item, value, sizeof(value))) {
    CmdOutPrintfLine(out, "%s = %s", item->key, value);
  } else {
    CmdOutPrintfLine(out, "%s = <unset>", item->key);
  }
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
  (void)args;

  for (size_t i = 0; i < ConfigConfigKeyCount(); i++) {
    const HxConfigKeyDef* item = ConfigConfigKeyAt(i);
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
    CmdOutWriteLine(out, "usage: readcfg <key>");
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
    CmdOutWriteLine(out, "usage: setcfg <key> <value>");
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

  if (!ConfigConfigSetValueFromString(item, value)) {
    CmdOutWriteLine(out, "invalid config value");
    return HX_CMD_ERROR;
  }

  ConfigApply();
  CmdOutPrintfLine(out, "%s updated", item->key);
  return HX_CMD_OK;
}

static HxCmdStatus CmdSaveConfig(const char* args, HxCmdOutput* out) {
  (void)args;

  if (ConfigSave()) {
    CmdOutWriteLine(out, "config saved to NVS");
    return HX_CMD_OK;
  }

  CmdOutWriteLine(out, "config save failed");
  return HX_CMD_ERROR;
}

static HxCmdStatus CmdLoadConfig(const char* args, HxCmdOutput* out) {
  (void)args;

  if (ConfigLoad()) {
    ConfigApply();
    CmdOutWriteLine(out, "config loaded from NVS");
    return HX_CMD_OK;
  }

  CmdOutWriteLine(out, "config load failed");
  return HX_CMD_ERROR;
}

static HxCmdStatus CmdDefaultConfig(const char* args, HxCmdOutput* out) {
  (void)args;

  ConfigResetToDefaults(&HxConfigData);
  ConfigApply();
  CmdOutWriteLine(out, "config reset to build defaults");
  return HX_CMD_OK;
}

static HxCmdStatus CmdListState(const char* args, HxCmdOutput* out) {
  (void)args;

  for (size_t i = 0; i < StateKeyCount(); i++) {
    const HxStateKeyDef* item = StateKeyAt(i);
    if (!item || !item->console_visible) {
      continue;
    }

    CmdWriteStateItemLine(item, out);
  }

  return HX_CMD_OK;
}

static HxCmdStatus CmdReadState(const char* args, HxCmdOutput* out) {
  char key[64];

  if (!CmdExtractSingleKey(args, key, sizeof(key))) {
    CmdOutWriteLine(out, "usage: readstate <key>");
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

static const HxCmdDef kBuiltinCommands[] = {
  { "help",            CmdHelp,          "Show command list" },
  { "?",               CmdHelp,          nullptr },
  { "reboot",          CmdReboot,        "Restart device" },
  { "log",             CmdLogHistory,    "Show log history" },
  { "logclr",          CmdLogClear,      "Clear log history" },
  { "logstat",         CmdLogStats,      "Show log statistics" },
  { "listcfg",         CmdListConfig,    "List visible config keys" },
  { "config",          CmdListConfig,    nullptr },
  { "show config",     CmdListConfig,    nullptr },
  { "readcfg",         CmdReadConfig,    "Read config key" },
  { "setcfg",          CmdSetConfig,     "Set config key" },
  { "savecfg",         CmdSaveConfig,    "Save config to NVS" },
  { "config save",     CmdSaveConfig,    nullptr },
  { "loadcfg",         CmdLoadConfig,    "Load config from NVS" },
  { "config load",     CmdLoadConfig,    nullptr },
  { "defaultcfg",      CmdDefaultConfig, "Reset config to defaults" },
  { "config defaults", CmdDefaultConfig, nullptr },
  { "liststate",       CmdListState,     "List visible state keys" },
  { "readstate",       CmdReadState,     "Read state key" }
};

bool CommandRegisterBuiltins() {
  for (size_t i = 0; i < (sizeof(kBuiltinCommands) / sizeof(kBuiltinCommands[0])); i++) {
    if (!CommandRegister(&kBuiltinCommands[i])) {
      return false;
    }
  }

  return true;
}