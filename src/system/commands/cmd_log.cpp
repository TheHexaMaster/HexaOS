/*
  HexaOS - cmd_log.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Log inspection commands for HexaOS.
  Registers: log, logclr, logstat.
*/

#include <stdlib.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "system/core/config.h"
#include "system/core/log.h"

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

  const char* level_names[] = { "error", "warn", "info", "debug", "lld" };
  HxLogLevel  current       = LogGetLevel();
  const char* level_str     = (current <= HX_LOG_LLD) ? level_names[current] : "unknown";

  CmdOutPrintfLine(out, "level    = %s", level_str);
  CmdOutPrintfLine(out,
                   "used=%lu capacity=%lu dropped_lines=%lu dropped_isr=%lu",
                   (unsigned long)LogHistorySize(),
                   (unsigned long)LogHistoryCapacity(),
                   (unsigned long)LogDroppedLines(),
                   (unsigned long)LogDroppedIsr());
  return HX_CMD_OK;
}

static HxCmdStatus CmdLogLevel(const char* args, HxCmdOutput* out) {
  static const char* kNames[] = { "error", "warn", "info", "debug", "lld" };

  char token[16];
  if (!CmdExtractSingleKey(args, token, sizeof(token))) {
    HxLogLevel current = LogGetLevel();
    CmdOutPrintfLine(out, "level = %s",
                     (current <= HX_LOG_LLD) ? kNames[current] : "unknown");
    CmdOutWriteLine(out, "usage: log level <error|warn|info|debug|lld>");
    return HX_CMD_OK;
  }

  int32_t level_int = -1;
  for (int i = 0; i <= (int)HX_LOG_LLD; i++) {
    if (strcmp(token, kNames[i]) == 0) { level_int = i; break; }
  }
  if (level_int < 0) {
    CmdOutWriteLine(out, "unknown level — use: error warn info debug lld");
    return HX_CMD_USAGE;
  }

  char level_str[4];
  snprintf(level_str, sizeof(level_str), "%d", (int)level_int);
  const HxConfigKeyDef* item = ConfigFindConfigKey(HX_CFG_log_level);
  if (!item || !ConfigSetValueFromString(item, level_str)) {
    CmdOutWriteLine(out, "config update failed");
    return HX_CMD_ERROR;
  }
  ConfigApply();
  if (!ConfigSave()) {
    CmdOutPrintfLine(out, "log level = %s (config save failed)", token);
    return HX_CMD_ERROR;
  }
  CmdOutPrintfLine(out, "log level = %s (saved)", token);
  return HX_CMD_OK;
}

static const HxCmdDef kLogCommands[] = {
  { "log",       CmdLogHistory, "Show log history" },
  { "logclr",    CmdLogClear,   "Clear log history" },
  { "logstat",   CmdLogStats,   "Show log level and statistics" },
  { "log level", CmdLogLevel,   "Get or set log level: log level <error|warn|info|debug|lld>" }
};

bool CommandRegisterLog() {
  for (size_t i = 0; i < (sizeof(kLogCommands) / sizeof(kLogCommands[0])); i++) {
    if (!CommandRegister(&kLogCommands[i])) {
      return false;
    }
  }
  return true;
}
