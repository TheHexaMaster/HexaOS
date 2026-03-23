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

  CmdOutPrintfLine(out,
                   "log: used=%lu capacity=%lu dropped_lines=%lu dropped_isr=%lu",
                   (unsigned long)LogHistorySize(),
                   (unsigned long)LogHistoryCapacity(),
                   (unsigned long)LogDroppedLines(),
                   (unsigned long)LogDroppedIsr());
  return HX_CMD_OK;
}

static const HxCmdDef kLogCommands[] = {
  { "log",     CmdLogHistory, "Show log history" },
  { "logclr",  CmdLogClear,   "Clear log history" },
  { "logstat", CmdLogStats,   "Show log statistics" }
};

bool CommandRegisterLog() {
  for (size_t i = 0; i < (sizeof(kLogCommands) / sizeof(kLogCommands[0])); i++) {
    if (!CommandRegister(&kLogCommands[i])) {
      return false;
    }
  }
  return true;
}
