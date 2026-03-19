/*
  HexaOS - mod_console.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Interactive serial console module.
  Implements the HexaOS shell prompt, line editing, serial input handling and
  prompt-preserving integration with the logging backend while delegating
  command execution to the shared command engine.
*/

#include "hexaos.h"
#include "system/adapters/console_adapter.h"
#include "system/commands/command_engine.h"

#include <stdio.h>
#include <string.h>

static constexpr size_t HX_CONSOLE_LINE_MAX = 128;
static constexpr const char* HX_CONSOLE_PROMPT = "hx> ";
static constexpr size_t HX_CONSOLE_PROMPT_LEN = 4;

static portMUX_TYPE g_console_state_mux = portMUX_INITIALIZER_UNLOCKED;

static char g_console_line[HX_CONSOLE_LINE_MAX];
static size_t g_console_len = 0;
static bool g_console_overflow = false;
static bool g_last_was_cr = false;
static bool g_console_editing_active = false;

static void ConsoleCommandWriteRaw(void* user, const char* text) {
  (void)user;
  LogSinkWriteRaw(text ? text : "");
}

static void ConsoleCommandWriteLine(void* user, const char* text) {
  (void)user;
  LogSinkWriteLineRaw(text ? text : "");
}

static HxCmdOutput g_console_cmd_output = {
  .write_raw = ConsoleCommandWriteRaw,
  .write_line = ConsoleCommandWriteLine,
  .user = nullptr,
  .interactive = true
};

static void ConsoleSetEditingActive(bool active) {
  taskENTER_CRITICAL(&g_console_state_mux);
  g_console_editing_active = active;
  taskEXIT_CRITICAL(&g_console_state_mux);
}

static void ConsoleClearLine() {
  taskENTER_CRITICAL(&g_console_state_mux);
  memset(g_console_line, 0, sizeof(g_console_line));
  g_console_len = 0;
  g_console_overflow = false;
  taskEXIT_CRITICAL(&g_console_state_mux);
}

static bool ConsoleSnapshotLine(char* out, size_t out_size, size_t* out_len) {
  bool active;
  size_t len;

  taskENTER_CRITICAL(&g_console_state_mux);
  active = g_console_editing_active;
  len = g_console_len;

  if (out && (out_size > 0)) {
    size_t copy_len = len;
    if (copy_len >= out_size) {
      copy_len = out_size - 1;
    }
    memcpy(out, g_console_line, copy_len);
    out[copy_len] = '\0';
    len = copy_len;
  }

  taskEXIT_CRITICAL(&g_console_state_mux);

  if (out_len) {
    *out_len = len;
  }

  return active;
}

static bool ConsoleAppendChar(char ch) {
  bool accepted = false;

  taskENTER_CRITICAL(&g_console_state_mux);

  if (!g_console_overflow && (g_console_len < (HX_CONSOLE_LINE_MAX - 1))) {
    g_console_line[g_console_len++] = ch;
    g_console_line[g_console_len] = '\0';
    accepted = true;
  } else {
    g_console_overflow = true;
  }

  taskEXIT_CRITICAL(&g_console_state_mux);
  return accepted;
}

static bool ConsoleBackspace() {
  bool removed = false;

  taskENTER_CRITICAL(&g_console_state_mux);

  if (!g_console_overflow && (g_console_len > 0)) {
    g_console_len--;
    g_console_line[g_console_len] = '\0';
    removed = true;
  }

  taskEXIT_CRITICAL(&g_console_state_mux);
  return removed;
}

static void ConsolePrompt() {
  LogSinkWriteRaw(HX_CONSOLE_PROMPT);
  ConsoleSetEditingActive(true);
}

static void ConsoleOnSinkLockedPreWriteLine() {
  char snapshot[HX_CONSOLE_LINE_MAX];
  size_t len = 0;
  bool active = ConsoleSnapshotLine(snapshot, sizeof(snapshot), &len);

  if (!active) {
    return;
  }

  ConsoleAdapterWriteChar('\r');
  for (size_t i = 0; i < (HX_CONSOLE_PROMPT_LEN + len); i++) {
    ConsoleAdapterWriteChar(' ');
  }
  ConsoleAdapterWriteChar('\r');
}

static void ConsoleOnSinkLockedPostWriteLine() {
  char snapshot[HX_CONSOLE_LINE_MAX];
  size_t len = 0;
  bool active = ConsoleSnapshotLine(snapshot, sizeof(snapshot), &len);

  if (!active) {
    return;
  }

  ConsoleAdapterWriteText(HX_CONSOLE_PROMPT);
  if (len > 0) {
    ConsoleAdapterWriteData((const uint8_t*)snapshot, len);
  }
}

static void ConsoleHandleLine() {
  char line[HX_CONSOLE_LINE_MAX];
  bool overflow;
  size_t len;

  taskENTER_CRITICAL(&g_console_state_mux);
  overflow = g_console_overflow;
  len = g_console_len;
  if (len >= sizeof(line)) {
    len = sizeof(line) - 1;
  }
  memcpy(line, g_console_line, len);
  line[len] = '\0';
  taskEXIT_CRITICAL(&g_console_state_mux);

  if (overflow) {
    LogWarn("CON: input too long");
    ConsoleClearLine();
    ConsolePrompt();
    return;
  }

  size_t start = 0;
  while ((start < len) && ((line[start] == ' ') || (line[start] == '\t'))) {
    start++;
  }

  size_t end = len;
  while ((end > start) && ((line[end - 1] == ' ') || (line[end - 1] == '\t'))) {
    end--;
  }

  line[end] = '\0';
  ConsoleClearLine();
  CommandExecuteLine(&line[start], &g_console_cmd_output);
  ConsolePrompt();
}

static void ConsoleReadSerial() {
  while (true) {
    int ch = ConsoleAdapterReadByte();
    if (ch < 0) {
      break;
    }

    if (ch == '\n') {
      if (g_last_was_cr) {
        g_last_was_cr = false;
        continue;
      }

      ConsoleSetEditingActive(false);
      LogSinkWriteRaw("\r\n");
      ConsoleHandleLine();
      continue;
    }

    if (ch == '\r') {
      g_last_was_cr = true;
      ConsoleSetEditingActive(false);
      LogSinkWriteRaw("\r\n");
      ConsoleHandleLine();
      continue;
    }

    g_last_was_cr = false;

    if ((ch == 0x08) || (ch == 0x7F)) {
      if (ConsoleBackspace()) {
        LogSinkWriteRaw("\b \b");
      }
      continue;
    }

    if ((ch < 32) || (ch > 126)) {
      continue;
    }

    if (ConsoleAppendChar((char)ch)) {
      LogSinkWriteChar((char)ch);
    }
  }
}

static bool ConsoleInit() {
  ConsoleClearLine();
  ConsoleSetEditingActive(false);
  LogSetSinkLineHooks(ConsoleOnSinkLockedPreWriteLine, ConsoleOnSinkLockedPostWriteLine);
  LogInfo("CON: init");
  return true;
}

static void ConsoleStart() {
  LogInfo("CON: start");
  LogInfo("CON: commands available: help, reboot, log, logclr, logstat, listcfg, readcfg, setcfg, savecfg, loadcfg, defaultcfg, liststate, readstate");
  ConsolePrompt();
}

static void ConsoleLoop() {
  ConsoleReadSerial();
}

static void ConsoleEvery100ms() {
}

static void ConsoleEverySecond() {
}

const HxModule ModuleConsole = {
  .name = "console",
  .init = ConsoleInit,
  .start = ConsoleStart,
  .loop = ConsoleLoop,
  .every_100ms = ConsoleEvery100ms,
  .every_1s = ConsoleEverySecond
};