/*
  HexaOS - user_interface_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared shell/session handler for the HexaOS user interface layer.
  Implements prompt rendering, line editing, command execution and log-aware
  redraw logic while delegating all transport I/O to callbacks injected by the
  core user interface runtime bridge.
*/

#include "hexaos.h"
#include "system/commands/command_engine.h"
#include "system/handlers/user_interface_handler.h"

#include <stdio.h>
#include <string.h>

static constexpr size_t HX_UI_LINE_MAX = 128;
static constexpr const char* HX_UI_PROMPT = "hx> ";
static constexpr size_t HX_UI_PROMPT_LEN = 4;

static HxRtosCritical g_ui_state_critical = HX_RTOS_CRITICAL_INIT;
static HxUserInterfaceWriteOps g_ui_write_ops = {};

static char g_ui_line[HX_UI_LINE_MAX];
static size_t g_ui_len = 0;
static bool g_ui_overflow = false;
static bool g_ui_last_was_cr = false;
static bool g_ui_editing_active = false;

static void UiStateEnter() {
  RtosCriticalEnter(&g_ui_state_critical);
}

static void UiStateExit() {
  RtosCriticalExit(&g_ui_state_critical);
}

static size_t UiWriteData(const uint8_t* data, size_t len) {
  if (!g_ui_write_ops.write_data) {
    return 0;
  }
  return g_ui_write_ops.write_data(data, len);
}

static size_t UiWriteText(const char* text) {
  if (!g_ui_write_ops.write_text) {
    return 0;
  }
  return g_ui_write_ops.write_text(text);
}

static size_t UiWriteChar(char ch) {
  if (!g_ui_write_ops.write_char) {
    return 0;
  }
  return g_ui_write_ops.write_char(ch);
}

static void UiFlush() {
  if (g_ui_write_ops.flush) {
    g_ui_write_ops.flush();
  }
}

static void UiCommandWriteRaw(void* user, const char* text) {
  (void)user;
  LogSinkWriteRaw(text ? text : "");
}

static void UiCommandWriteLine(void* user, const char* text) {
  (void)user;
  LogSinkWriteLineRaw(text ? text : "");
}

static HxCmdOutput g_ui_cmd_output = {
  .write_raw = UiCommandWriteRaw,
  .write_line = UiCommandWriteLine,
  .user = nullptr,
  .interactive = true
};

static void UiSetEditingActive(bool active) {
  UiStateEnter();
  g_ui_editing_active = active;
  UiStateExit();
}

static void UiClearLine() {
  UiStateEnter();
  memset(g_ui_line, 0, sizeof(g_ui_line));
  g_ui_len = 0;
  g_ui_overflow = false;
  UiStateExit();
}

static bool UiSnapshotLine(char* out, size_t out_size, size_t* out_len) {
  bool active;
  size_t len;

  UiStateEnter();
  active = g_ui_editing_active;
  len = g_ui_len;

  if (out && (out_size > 0)) {
    size_t copy_len = len;
    if (copy_len >= out_size) {
      copy_len = out_size - 1;
    }
    memcpy(out, g_ui_line, copy_len);
    out[copy_len] = '\0';
    len = copy_len;
  }

  UiStateExit();

  if (out_len) {
    *out_len = len;
  }

  return active;
}

static bool UiAppendChar(char ch) {
  bool accepted = false;

  UiStateEnter();

  if (!g_ui_overflow && (g_ui_len < (HX_UI_LINE_MAX - 1))) {
    g_ui_line[g_ui_len++] = ch;
    g_ui_line[g_ui_len] = '\0';
    accepted = true;
  } else {
    g_ui_overflow = true;
  }

  UiStateExit();
  return accepted;
}

static bool UiBackspace() {
  bool removed = false;

  UiStateEnter();

  if (!g_ui_overflow && (g_ui_len > 0)) {
    g_ui_len--;
    g_ui_line[g_ui_len] = '\0';
    removed = true;
  }

  UiStateExit();
  return removed;
}

static void UiPrompt() {
  LogSinkWriteRaw(HX_UI_PROMPT);
  UiSetEditingActive(true);
}

static void UiOnSinkLockedPreWriteLine() {
  char snapshot[HX_UI_LINE_MAX];
  size_t len = 0;
  bool active = UiSnapshotLine(snapshot, sizeof(snapshot), &len);

  if (!active) {
    return;
  }

  UiWriteChar('\r');
  for (size_t i = 0; i < (HX_UI_PROMPT_LEN + len); i++) {
    UiWriteChar(' ');
  }
  UiWriteChar('\r');
}

static void UiOnSinkLockedPostWriteLine() {
  char snapshot[HX_UI_LINE_MAX];
  size_t len = 0;
  bool active = UiSnapshotLine(snapshot, sizeof(snapshot), &len);

  if (!active) {
    return;
  }

  UiWriteText(HX_UI_PROMPT);
  if (len > 0) {
    UiWriteData((const uint8_t*)snapshot, len);
  }
}

static void UiHandleLine() {
  char line[HX_UI_LINE_MAX];
  bool overflow;
  size_t len;

  UiStateEnter();
  overflow = g_ui_overflow;
  len = g_ui_len;
  if (len >= sizeof(line)) {
    len = sizeof(line) - 1;
  }
  memcpy(line, g_ui_line, len);
  line[len] = '\0';
  UiStateExit();

  if (overflow) {
    HX_LOGW("UI", "input too long");
    UiClearLine();
    UiPrompt();
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
  UiClearLine();
  CommandExecuteLine(&line[start], &g_ui_cmd_output);
  UiPrompt();
}

bool UserInterfaceHandlerInit(const HxUserInterfaceWriteOps* ops) {
  if (!ops || !ops->write_data || !ops->write_text || !ops->write_char) {
    HX_LOGE("UI", "invalid write ops");
    return false;
  }

  if (!RtosCriticalReady(&g_ui_state_critical) && !RtosCriticalInit(&g_ui_state_critical)) {
    HX_LOGE("UI", "state critical init failed");
    return false;
  }

  g_ui_write_ops = *ops;
  UiClearLine();
  UiSetEditingActive(false);
  g_ui_last_was_cr = false;
  LogSetSinkLineHooks(UiOnSinkLockedPreWriteLine, UiOnSinkLockedPostWriteLine);
  HX_LOGI("UI", "handler init");
  return true;
}

void UserInterfaceHandlerStart() {
  HX_LOGI("UI", "handler start. Use help command to list available commands.");
  UiPrompt();
}

void UserInterfaceHandlerHandleByte(int ch) {
  if (ch < 0) {
    return;
  }

  if (ch == '\n') {
    if (g_ui_last_was_cr) {
      g_ui_last_was_cr = false;
      return;
    }

    UiSetEditingActive(false);
    LogSinkWriteRaw("\r\n");
    UiHandleLine();
    return;
  }

  if (ch == '\r') {
    g_ui_last_was_cr = true;
    UiSetEditingActive(false);
    LogSinkWriteRaw("\r\n");
    UiHandleLine();
    return;
  }

  g_ui_last_was_cr = false;

  if ((ch == 0x08) || (ch == 0x7F)) {
    if (UiBackspace()) {
      LogSinkWriteRaw("\b \b");
    }
    return;
  }

  if ((ch < 32) || (ch > 126)) {
    return;
  }

  if (UiAppendChar((char)ch)) {
    LogSinkWriteChar((char)ch);
  }

  UiFlush();
}
