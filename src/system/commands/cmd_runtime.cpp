/*
  HexaOS - cmd_runtime.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Runtime introspection commands for HexaOS.
  Registers: runtime status, module list, module info <name>.
*/

#include <stdio.h>
#include <string.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "system/core/module_registry.h"
#include "system/core/runtime.h"
#include "system/core/time.h"

static HxCmdStatus CmdRuntimeStatus(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: runtime status");
    return HX_CMD_USAGE;
  }

  char uptime_text[32];
  TimeFormatMonotonic(uptime_text, sizeof(uptime_text), (uint64_t)Hx.uptime_ms);

  CmdOutWriteLine(out, "runtime:");
  CmdOutPrintfLine(out, "  rtos_ready       = %s", Hx.rtos_ready       ? "true" : "false");
  CmdOutPrintfLine(out, "  time_ready       = %s", Hx.time_ready        ? "true" : "false");
  CmdOutPrintfLine(out, "  config_loaded    = %s", Hx.config_loaded     ? "true" : "false");
  CmdOutPrintfLine(out, "  pinmap_ready     = %s", Hx.pinmap_ready      ? "true" : "false");
  CmdOutPrintfLine(out, "  state_loaded     = %s", Hx.state_loaded      ? "true" : "false");
  CmdOutPrintfLine(out, "  files_mounted    = %s", Hx.files_mounted     ? "true" : "false");
  CmdOutPrintfLine(out, "  sd_mounted       = %s", Hx.sd_mounted        ? "true" : "false");
  CmdOutPrintfLine(out, "  safeboot         = %s", Hx.safeboot          ? "true" : "false");
  CmdOutPrintfLine(out, "  uptime           = %s", uptime_text);
  CmdOutPrintfLine(out, "  boot_count       = %lu", (unsigned long)Hx.boot_count);
  CmdOutPrintfLine(out, "  modules          = %lu", (unsigned long)ModuleRegisteredCount());
  return HX_CMD_OK;
}

static HxCmdStatus CmdModuleList(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: module list");
    return HX_CMD_USAGE;
  }

  size_t count = ModuleRegisteredCount();

  if (count == 0) {
    CmdOutWriteLine(out, "no modules registered");
    return HX_CMD_OK;
  }

  for (size_t i = 0; i < count; i++) {
    const HxModuleRecord* rec = ModuleRecordAt(i);
    if (!rec) {
      continue;
    }
    CmdOutPrintfLine(out,
                     "  %-16s  ready=%-5s  started=%s",
                     rec->name    ? rec->name : "?",
                     rec->ready   ? "true"    : "false",
                     rec->started ? "true"    : "false");
  }

  return HX_CMD_OK;
}

static HxCmdStatus CmdModuleInfo(const char* args, HxCmdOutput* out) {
  char name[32];

  if (!CmdExtractSingleKey(args, name, sizeof(name))) {
    CmdOutWriteLine(out, "usage: module info <name>");
    return HX_CMD_USAGE;
  }

  size_t count = ModuleRegisteredCount();
  for (size_t i = 0; i < count; i++) {
    const HxModuleRecord* rec = ModuleRecordAt(i);
    if (!rec || !rec->name) {
      continue;
    }
    if (strcmp(rec->name, name) != 0) {
      continue;
    }

    CmdOutPrintfLine(out, "name    = %s", rec->name);
    CmdOutPrintfLine(out, "ready   = %s", rec->ready   ? "true" : "false");
    CmdOutPrintfLine(out, "started = %s", rec->started ? "true" : "false");
    return HX_CMD_OK;
  }

  CmdOutWriteLine(out, "module not found");
  return HX_CMD_ERROR;
}

static const HxCmdDef kRuntimeCommands[] = {
  { "runtime",        CmdRuntimeStatus, "Show runtime system status" },
  { "runtime status", CmdRuntimeStatus, nullptr },
  { "module list",    CmdModuleList,    "List registered modules and their status" },
  { "module info",    CmdModuleInfo,    "Show status of one module by name" }
};

bool CommandRegisterRuntime() {
  for (size_t i = 0; i < (sizeof(kRuntimeCommands) / sizeof(kRuntimeCommands[0])); i++) {
    if (!CommandRegister(&kRuntimeCommands[i])) {
      return false;
    }
  }
  return true;
}
