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

#include "system/commands/command_engine.h"
#include "system/commands/command_builtin.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static constexpr size_t HX_COMMAND_MAX_COUNT = 32;
static constexpr size_t HX_COMMAND_LINE_MAX = 192;

static const HxCmdDef* g_commands[HX_COMMAND_MAX_COUNT];
static size_t g_command_count = 0;

static const HxCmdDef* CommandFindExact(const char* name) {
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

static const HxCmdDef* CommandFindBestPrefix(const char* line, size_t* matched_len_out) {
  if (matched_len_out) {
    *matched_len_out = 0;
  }

  if (!line || !line[0]) {
    return nullptr;
  }

  const HxCmdDef* best = nullptr;
  size_t best_len = 0;

  for (size_t i = 0; i < g_command_count; i++) {
    const HxCmdDef* def = g_commands[i];
    if (!def || !def->name || !def->name[0]) {
      continue;
    }

    size_t name_len = strlen(def->name);
    if (name_len == 0) {
      continue;
    }

    if (strncmp(line, def->name, name_len) != 0) {
      continue;
    }

    char next = line[name_len];
    if ((next != '\0') && (next != ' ') && (next != '\t')) {
      continue;
    }

    if (name_len > best_len) {
      best = def;
      best_len = name_len;
    }
  }

  if (matched_len_out) {
    *matched_len_out = best_len;
  }

  return best;
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

  if (CommandFindExact(def->name)) {
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

  size_t matched_len = 0;
  const HxCmdDef* def = CommandFindBestPrefix(start, &matched_len);
  if (!def) {
    char unknown[64];
    size_t i = 0;

    while (start[i] && (start[i] != ' ') && (start[i] != '\t') && (i < (sizeof(unknown) - 1))) {
      unknown[i] = start[i];
      i++;
    }
    unknown[i] = '\0';

    CmdOutPrintfLine(out, "unknown command: %s", unknown[0] ? unknown : start);
    return HX_CMD_UNKNOWN;
  }

  char* args = start + matched_len;
  while ((*args == ' ') || (*args == '\t')) {
    args++;
  }

  return def->handler(args, out);
}