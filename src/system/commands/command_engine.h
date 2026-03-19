/*
  HexaOS - command_engine.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared command engine for HexaOS.
  Provides a frontend-agnostic registry and execution API so serial, web or
  other user interfaces can run the same commands without duplicating parsing
  or business logic.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

enum HxCmdStatus : uint8_t {
  HX_CMD_OK = 0,
  HX_CMD_ERROR = 1,
  HX_CMD_USAGE = 2,
  HX_CMD_UNKNOWN = 3
};

typedef struct {
  void (*write_raw)(void* user, const char* text);
  void (*write_line)(void* user, const char* text);
  void* user;
  bool interactive;
} HxCmdOutput;

typedef HxCmdStatus (*HxCmdHandler)(const char* args, HxCmdOutput* out);

typedef struct {
  const char* name;
  HxCmdHandler handler;
  const char* help;
} HxCmdDef;

bool CommandInit();
bool CommandRegister(const HxCmdDef* def);
size_t CommandCount();
const HxCmdDef* CommandAt(size_t index);
HxCmdStatus CommandExecuteLine(const char* line, HxCmdOutput* out);

void CmdOutWriteRaw(HxCmdOutput* out, const char* text);
void CmdOutWriteLine(HxCmdOutput* out, const char* text);
void CmdOutPrintfLine(HxCmdOutput* out, const char* fmt, ...);