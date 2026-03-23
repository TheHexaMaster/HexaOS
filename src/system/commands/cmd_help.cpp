/*
  HexaOS - cmd_help.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Help and system control commands for HexaOS.
  Registers: help, ?, reboot.
*/

#include <esp32-hal.h>
#include <esp_system.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "system/core/log.h"

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

static const HxCmdDef kHelpCommands[] = {
  { "help",   CmdHelp,   "Show command list" },
  { "?",      CmdHelp,   nullptr },
  { "reboot", CmdReboot, "Restart device" }
};

bool CommandRegisterHelp() {
  for (size_t i = 0; i < (sizeof(kHelpCommands) / sizeof(kHelpCommands[0])); i++) {
    if (!CommandRegister(&kHelpCommands[i])) {
      return false;
    }
  }
  return true;
}
