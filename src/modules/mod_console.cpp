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

static void ConsoleWriteConfigItemLine(const HxConfigKeyDef* item) {
  if (!item) {
    return;
  }

  char current[96];
  char defaults[96];

  if (!SetupConfigValueToString(item, current, sizeof(current))) {
    snprintf(current, sizeof(current), "<error>");
  }

  if (!SetupConfigDefaultToString(item, defaults, sizeof(defaults))) {
    snprintf(defaults, sizeof(defaults), "<error>");
  }

  char line[256];
  snprintf(line, sizeof(line), "%s = %s (default=%s)", item->key, current, defaults);
  LogSinkWriteLineRaw(line);
}

static void ConsoleWriteStateItemLine(const HxStateKeyDef* item) {
  if (!item) {
    return;
  }

  char value[96];
  char line[256];

  if (StateValueToString(item, value, sizeof(value))) {
    snprintf(line, sizeof(line), "%s = %s", item->key, value);
  } else {
    snprintf(line, sizeof(line), "%s = <unset>", item->key);
  }

  LogSinkWriteLineRaw(line);
}

static void ConsolePrintHelp() {
  LogSinkWriteLineRaw("commands:");
  LogSinkWriteLineRaw("  help");
  LogSinkWriteLineRaw("  reboot");
  LogSinkWriteLineRaw("  log");
  LogSinkWriteLineRaw("  logclr");
  LogSinkWriteLineRaw("  logstat");
  LogSinkWriteLineRaw("  listcfg");
  LogSinkWriteLineRaw("  readcfg <key>");
  LogSinkWriteLineRaw("  setcfg <key> <value>");
  LogSinkWriteLineRaw("  savecfg");
  LogSinkWriteLineRaw("  loadcfg");
  LogSinkWriteLineRaw("  defaultcfg");
  LogSinkWriteLineRaw("  liststate");
  LogSinkWriteLineRaw("  readstate <key>");
  ConsolePrompt();
}

static void ConsolePrintConfigList() {
  for (size_t i = 0; i < SetupConfigKeyCount(); i++) {
    const HxConfigKeyDef* item = SetupConfigKeyAt(i);
    if (!item || !item->console_visible) {
      continue;
    }

    ConsoleWriteConfigItemLine(item);
  }

  char line[96];
  snprintf(line, sizeof(line), "config.loaded = %s", Hx.config_loaded ? "true" : "false");
  LogSinkWriteLineRaw(line);
  ConsolePrompt();
}

static void ConsolePrintStateList() {
  for (size_t i = 0; i < StateKeyCount(); i++) {
    const HxStateKeyDef* item = StateKeyAt(i);
    if (!item || !item->console_visible) {
      continue;
    }

    ConsoleWriteStateItemLine(item);
  }

  ConsolePrompt();
}

static void ConsoleReadConfigKey(const char* key) {
  const HxConfigKeyDef* item = SetupFindConfigKey(key);
  if (!item) {
    LogSinkWriteLineRaw("config key not found");
    ConsolePrompt();
    return;
  }

  ConsoleWriteConfigItemLine(item);
  ConsolePrompt();
}

static void ConsoleReadStateKey(const char* key) {
  const HxStateKeyDef* item = StateFindKey(key);
  if (!item) {
    LogSinkWriteLineRaw("state key not found");
    ConsolePrompt();
    return;
  }

  ConsoleWriteStateItemLine(item);
  ConsolePrompt();
}

static void ConsoleSetConfigKey(const char* key, const char* value) {
  const HxConfigKeyDef* item = SetupFindConfigKey(key);
  if (!item) {
    LogSinkWriteLineRaw("config key not found");
    ConsolePrompt();
    return;
  }

  if (!item->console_writable) {
    LogSinkWriteLineRaw("config key is read-only");
    ConsolePrompt();
    return;
  }

  if (!SetupConfigSetValueFromString(item, value)) {
    LogSinkWriteLineRaw("invalid config value");
    ConsolePrompt();
    return;
  }

  SetupApply();

  char line[192];
  snprintf(line, sizeof(line), "%s updated", item->key);
  LogSinkWriteLineRaw(line);
  ConsolePrompt();
}

static bool ConsoleSplitKeyValue(const char* text, char* key_out, size_t key_size, const char** value_out) {
  if (!text || !key_out || (key_size == 0) || !value_out) {
    return false;
  }

  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }

  if (!text[0]) {
    return false;
  }

  const char* sep = text;
  while (*sep && (*sep != ' ') && (*sep != '\t')) {
    sep++;
  }

  size_t key_len = (size_t)(sep - text);
  if ((key_len == 0) || (key_len >= key_size)) {
    return false;
  }

  memcpy(key_out, text, key_len);
  key_out[key_len] = '\0';

  while ((*sep == ' ') || (*sep == '\t')) {
    sep++;
  }

  *value_out = sep;
  return (sep[0] != '\0');
}

static bool ConsoleExtractSingleKey(const char* text, char* key_out, size_t key_size) {
  if (!text || !key_out || (key_size == 0)) {
    return false;
  }

  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }

  if (!text[0]) {
    return false;
  }

  const char* end = text;
  while (*end && (*end != ' ') && (*end != '\t')) {
    end++;
  }

  while ((*end == ' ') || (*end == '\t')) {
    end++;
  }

  if (*end != '\0') {
    return false;
  }

  size_t len = (size_t)(end - text);
  while ((len > 0) && ((text[len - 1] == ' ') || (text[len - 1] == '\t'))) {
    len--;
  }

  if ((len == 0) || (len >= key_size)) {
    return false;
  }

  memcpy(key_out, text, len);
  key_out[len] = '\0';
  return true;
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

  if ((strcmp(line, "listcfg") == 0) ||
      (strcmp(line, "setup") == 0) ||
      (strcmp(line, "show setup") == 0)) {
    ConsolePrintConfigList();
    return;
  }

  if ((strcmp(line, "savecfg") == 0) || (strcmp(line, "setup save") == 0)) {
    if (SetupSave()) {
      LogSinkWriteLineRaw("config saved to NVS");
    } else {
      LogSinkWriteLineRaw("config save failed");
    }
    ConsolePrompt();
    return;
  }

  if ((strcmp(line, "loadcfg") == 0) || (strcmp(line, "setup load") == 0)) {
    if (SetupLoad()) {
      SetupApply();
      LogSinkWriteLineRaw("config loaded from NVS");
    } else {
      LogSinkWriteLineRaw("config load failed");
    }
    ConsolePrompt();
    return;
  }

  if ((strcmp(line, "defaultcfg") == 0) || (strcmp(line, "setup defaults") == 0)) {
    SetupResetToDefaults(&HxSetupData);
    SetupApply();
    LogSinkWriteLineRaw("config reset to build defaults");
    ConsolePrompt();
    return;
  }

  if (strncmp(line, "readcfg ", 8) == 0) {
    char key[64];
    if (!ConsoleExtractSingleKey(line + 8, key, sizeof(key))) {
      LogSinkWriteLineRaw("usage: readcfg <key>");
      ConsolePrompt();
      return;
    }

    ConsoleReadConfigKey(key);
    return;
  }

  if (strncmp(line, "setcfg ", 7) == 0) {
    char key[64];
    const char* value = nullptr;
    if (!ConsoleSplitKeyValue(line + 7, key, sizeof(key), &value)) {
      LogSinkWriteLineRaw("usage: setcfg <key> <value>");
      ConsolePrompt();
      return;
    }

    ConsoleSetConfigKey(key, value);
    return;
  }

  if (strcmp(line, "liststate") == 0) {
    ConsolePrintStateList();
    return;
  }

  if (strncmp(line, "readstate ", 10) == 0) {
    char key[64];
    if (!ConsoleExtractSingleKey(line + 10, key, sizeof(key))) {
      LogSinkWriteLineRaw("usage: readstate <key>");
      ConsolePrompt();
      return;
    }

    ConsoleReadStateKey(key);
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
  LogInfo("CON: commands available: help, reboot, log, logclr, logstat, listcfg, readcfg, setcfg, savecfg, loadcfg, defaultcfg, liststate, readstate");
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