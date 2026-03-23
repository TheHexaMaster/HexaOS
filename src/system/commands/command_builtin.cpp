/*
  HexaOS - command_builtin.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Built-in command registration entry point for HexaOS.
  Delegates to domain-specific registration functions in cmd_*.cpp files.
  Each domain file owns its own command handlers and registration table.
*/

#include "command_builtin.h"

bool CommandRegisterHelp();
bool CommandRegisterLog();
bool CommandRegisterTime();
bool CommandRegisterConfig();
bool CommandRegisterState();
bool CommandRegisterPinmap();
bool CommandRegisterRuntime();
bool CommandRegisterFiles();
bool CommandRegisterNetwork();

bool CommandRegisterBuiltins() {
  return CommandRegisterHelp()
      && CommandRegisterLog()
      && CommandRegisterTime()
      && CommandRegisterConfig()
      && CommandRegisterState()
      && CommandRegisterPinmap()
      && CommandRegisterRuntime()
      && CommandRegisterFiles()
      && CommandRegisterNetwork();
}
