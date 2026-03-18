/*
  HexaOS - mod_console.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Interactive serial console module.
  Implements the HexaOS shell prompt, line editing, command handling and prompt-preserving integration with the logging backend for debugging and service operations.
*/

#include "hexaos.h"

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static constexpr size_t HX_CONSOLE_LINE_MAX = 128;
static constexpr const char* HX_CONSOLE_PROMPT = "hx> ";
static constexpr size_t HX_CONSOLE_PROMPT_LEN = 4;

static portMUX_TYPE g_console_state_mux = portMUX_INITIALIZER_UNLOCKED;

static char g_console_line[HX_CONSOLE_LINE_MAX];
static size_t g_console_len = 0;
static bool g_console_overflow = false;
static bool g_last_was_cr = false;
static bool g_console_editing_active = false;

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

void ConsoleShowPrompt() {
  ConsolePrompt();
}

void ConsoleOnSinkLockedPreWriteLine() {
  char snapshot[HX_CONSOLE_LINE_MAX];
  size_t len = 0;
  bool active = ConsoleSnapshotLine(snapshot, sizeof(snapshot), &len);

  if (!active) {
    return;
  }

  Serial.write('\r');
  for (size_t i = 0; i < (HX_CONSOLE_PROMPT_LEN + len); i++) {
    Serial.write(' ');
  }
  Serial.write('\r');
}

void ConsoleOnSinkLockedPostWriteLine() {
  char snapshot[HX_CONSOLE_LINE_MAX];
  size_t len = 0;
  bool active = ConsoleSnapshotLine(snapshot, sizeof(snapshot), &len);

  if (!active) {
    return;
  }

  Serial.print(HX_CONSOLE_PROMPT);
  if (len > 0) {
    Serial.write((const uint8_t*)snapshot, len);
  }
}

static void ConsolePrintLogHistory() {
  size_t used = LogHistorySize();
  if (used == 0) {
    LogSinkWriteLineRaw("log history is empty");
    ConsolePrompt();
    return;
  }

  char* dump = (char*)malloc(used + 1);
  if (!dump) {
    LogSinkWriteLineRaw("log history dump failed: out of memory");
    ConsolePrompt();
    return;
  }

  size_t copied = LogHistoryCopy(dump, used + 1);
  if (copied > 0) {
    LogSinkWriteRaw(dump);
  }

  free(dump);
  ConsolePrompt();
}

static void ConsolePrintLogStats() {
  char line[160];
  snprintf(line, sizeof(line),
           "log: used=%lu capacity=%lu dropped_lines=%lu dropped_isr=%lu",
           (unsigned long)LogHistorySize(),
           (unsigned long)LogHistoryCapacity(),
           (unsigned long)LogDroppedLines(),
           (unsigned long)LogDroppedIsr());
  LogSinkWriteLineRaw(line);
  ConsolePrompt();
}

static void ConsolePrintHelp() {
  LogSinkWriteLineRaw("commands:");
  LogSinkWriteLineRaw("  help");
  LogSinkWriteLineRaw("  reboot");
  LogSinkWriteLineRaw("  log");
  LogSinkWriteLineRaw("  logclr");
  LogSinkWriteLineRaw("  logstat");
  LogSinkWriteLineRaw("  setup");
  LogSinkWriteLineRaw("  setup save");
  LogSinkWriteLineRaw("  setup load");
  LogSinkWriteLineRaw("  setup defaults");
  LogSinkWriteLineRaw("  set name <value>");
  LogSinkWriteLineRaw("  set log <error|warn|info|debug|0..3>");
  LogSinkWriteLineRaw("  set safeboot <on|off>");
  ConsolePrompt();
}

static void ConsolePrintSetup() {
  char line[160];

  snprintf(line, sizeof(line), "setup.device_name=%s", HxSetupData.device_name);
  LogSinkWriteLineRaw(line);

  snprintf(line, sizeof(line), "setup.log_level=%s (%d)",
           SetupLogLevelText(HxSetupData.log_level),
           (int)HxSetupData.log_level);
  LogSinkWriteLineRaw(line);

  snprintf(line, sizeof(line), "setup.safeboot_enable=%s",
           HxSetupData.safeboot_enable ? "true" : "false");
  LogSinkWriteLineRaw(line);

  snprintf(line, sizeof(line), "setup.config_loaded=%s",
           Hx.config_loaded ? "true" : "false");
  LogSinkWriteLineRaw(line);

  ConsolePrompt();
}

static bool ConsoleParseBool(const char* text, bool* value) {
  if (!text || !text[0] || !value) {
    return false;
  }

  if ((strcasecmp(text, "1") == 0) ||
      (strcasecmp(text, "on") == 0) ||
      (strcasecmp(text, "true") == 0) ||
      (strcasecmp(text, "yes") == 0)) {
    *value = true;
    return true;
  }

  if ((strcasecmp(text, "0") == 0) ||
      (strcasecmp(text, "off") == 0) ||
      (strcasecmp(text, "false") == 0) ||
      (strcasecmp(text, "no") == 0)) {
    *value = false;
    return true;
  }

  return false;
}

static void ConsoleCommandSetName(const char* value) {
  while (value && ((*value == ' ') || (*value == '\t'))) {
    value++;
  }

  if (SetupSetDeviceName(value)) {
    SetupApply();
    LogSinkWriteLineRaw("setup.device_name updated");
  } else {
    LogSinkWriteLineRaw("invalid device name");
  }

  ConsolePrompt();
}

static void ConsoleCommandSetLog(const char* value) {
  while (value && ((*value == ' ') || (*value == '\t'))) {
    value++;
  }

  HxLogLevel level;
  if (!SetupParseLogLevel(value, &level)) {
    LogSinkWriteLineRaw("invalid log level");
    ConsolePrompt();
    return;
  }

  if (!SetupSetLogLevel(level)) {
    LogSinkWriteLineRaw("failed to update log level");
    ConsolePrompt();
    return;
  }

  SetupApply();
  LogSinkWriteLineRaw("setup.log_level updated");
  ConsolePrompt();
}

static void ConsoleCommandSetSafeboot(const char* value) {
  while (value && ((*value == ' ') || (*value == '\t'))) {
    value++;
  }

  bool enabled = false;
  if (!ConsoleParseBool(value, &enabled)) {
    LogSinkWriteLineRaw("invalid safeboot value");
    ConsolePrompt();
    return;
  }

  SetupSetSafebootEnable(enabled);
  SetupApply();
  LogSinkWriteLineRaw("setup.safeboot_enable updated");
  ConsolePrompt();
}

static void ConsoleExecuteCommand(const char* line) {
  if (!line || !line[0]) {
    ConsolePrompt();
    return;
  }

  if ((strcmp(line, "help") == 0) || (strcmp(line, "?") == 0)) {
    ConsolePrintHelp();
    return;
  }

  if (strcmp(line, "reboot") == 0) {
    LogWarn("CON: soft restart requested");
    delay(100);
    esp_restart();
    return;
  }

  if (strcmp(line, "log") == 0) {
    ConsolePrintLogHistory();
    return;
  }

  if (strcmp(line, "logclr") == 0) {
    LogHistoryClear();
    LogSinkWriteLineRaw("log history cleared");
    ConsolePrompt();
    return;
  }

  if (strcmp(line, "logstat") == 0) {
    ConsolePrintLogStats();
    return;
  }

  if ((strcmp(line, "setup") == 0) || (strcmp(line, "show setup") == 0)) {
    ConsolePrintSetup();
    return;
  }

  if (strcmp(line, "setup save") == 0) {
    if (SetupSave()) {
      LogSinkWriteLineRaw("setup saved to NVS");
    } else {
      LogSinkWriteLineRaw("setup save failed");
    }
    ConsolePrompt();
    return;
  }

  if (strcmp(line, "setup load") == 0) {
    if (SetupLoad()) {
      SetupApply();
      LogSinkWriteLineRaw("setup loaded from NVS");
    } else {
      LogSinkWriteLineRaw("setup load failed");
    }
    ConsolePrompt();
    return;
  }

  if (strcmp(line, "setup defaults") == 0) {
    SetupResetToDefaults(&HxSetupData);
    SetupApply();
    LogSinkWriteLineRaw("setup reset to build defaults");
    ConsolePrompt();
    return;
  }

  if (strncmp(line, "set name ", 9) == 0) {
    ConsoleCommandSetName(line + 9);
    return;
  }

  if (strncmp(line, "set log ", 8) == 0) {
    ConsoleCommandSetLog(line + 8);
    return;
  }

  if (strncmp(line, "set safeboot ", 13) == 0) {
    ConsoleCommandSetSafeboot(line + 13);
    return;
  }

  LogWarn("CON: unknown command: %s", line);
  ConsolePrompt();
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
  ConsoleExecuteCommand(&line[start]);
}

static void ConsoleReadSerial() {
  while (Serial.available() > 0) {
    int ch = Serial.read();
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
  LogInfo("CON: init");
  return true;
}

static void ConsoleStart() {
  LogInfo("CON: start");
  LogInfo("CON: commands available: help, reboot, log, logclr, logstat, setup, set");
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