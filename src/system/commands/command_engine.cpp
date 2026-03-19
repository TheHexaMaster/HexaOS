/*
  HexaOS - command_engine.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared command engine for HexaOS.
  Provides a frontend-agnostic registry and execution API so serial, web or
  other user interfaces can run the same commands without duplicating parsing
  or business logic.
*/

#include "hexaos.h"
#include "system/commands/command_engine.h"
#include "system/commands/command_builtin.h"

#include <stdio.h>
#include <string.h>

static constexpr size_t HX_COMMAND_MAX_COUNT = 32;
static constexpr size_t HX_COMMAND_LINE_MAX = 192;

static const HxCmdDef* g_commands[HX_COMMAND_MAX_COUNT];
static size_t g_command_count = 0;

static const HxCmdDef* CommandFind(const char* name) {
  if (!name || !name[0]) {
    return nullptr;
  }

  for (size_t i = 0; i < g_command_count; i++) {
    const HxCmdDef* def = g_commands[i];
    if (!def || !def->name) {
      continue;
    }

    if (strcmp(def->name, name) == 0) {
      return def;
    }
  }

  return nullptr;
}

bool CommandInit() {
  memset(g_commands, 0, sizeof(g_commands));
  g_command_count = 0;
  return CommandRegisterBuiltins();
}

bool CommandRegister(const HxCmdDef* def) {
  if (!def || !def->name || !def->name[0] || !def->handler) {
    return false;
  }

  if (g_command_count >= HX_COMMAND_MAX_COUNT) {
    return false;
  }

  if (CommandFind(def->name)) {
    return false;
  }

  g_commands[g_command_count++] = def;
  return true;
}

size_t CommandCount() {
  return g_command_count;
}

const HxCmdDef* CommandAt(size_t index) {
  if (index >= g_command_count) {
    return nullptr;
  }

  return g_commands[index];
}

void CmdOutWriteRaw(HxCmdOutput* out, const char* text) {
  if (!out || !out->write_raw) {
    return;
  }

  out->write_raw(out->user, text ? text : "");
}

void CmdOutWriteLine(HxCmdOutput* out, const char* text) {
  if (!out) {
    return;
  }

  if (out->write_line) {
    out->write_line(out->user, text ? text : "");
    return;
  }

  if (out->write_raw) {
    out->write_raw(out->user, text ? text : "");
    out->write_raw(out->user, "\r\n");
  }
}

void CmdOutPrintfLine(HxCmdOutput* out, const char* fmt, ...) {
  if (!fmt) {
    return;
  }

  char line[256];

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);

  line[sizeof(line) - 1] = '\0';
  CmdOutWriteLine(out, line);
}

HxCmdStatus CommandExecuteLine(const char* line, HxCmdOutput* out) {
  if (!line) {
    return HX_CMD_ERROR;
  }

  size_t len = strlen(line);
  if (len >= HX_COMMAND_LINE_MAX) {
    CmdOutWriteLine(out, "command line too long");
    return HX_CMD_ERROR;
  }

  char local[HX_COMMAND_LINE_MAX];
  memcpy(local, line, len);
  local[len] = '\0';

  char* start = local;
  while ((*start == ' ') || (*start == '\t')) {
    start++;
  }

  if (!start[0]) {
    return HX_CMD_OK;
  }

  char* end = start + strlen(start);
  while ((end > start) && ((end[-1] == ' ') || (end[-1] == '\t'))) {
    end--;
  }
  *end = '\0';

  const HxCmdDef* full_def = CommandFind(start);
  if (full_def) {
    return full_def->handler("", out);
  }

  char* args = start;
  while (*args && (*args != ' ') && (*args != '\t')) {
    args++;
  }

  if (*args) {
    *args++ = '\0';
    while ((*args == ' ') || (*args == '\t')) {
      args++;
    }
  }

  const HxCmdDef* def = CommandFind(start);
  if (!def) {
    CmdOutPrintfLine(out, "unknown command: %s", start);
    return HX_CMD_UNKNOWN;
  }

  return def->handler(args, out);
}