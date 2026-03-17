#include "hexaos.h"
#include <esp_system.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static constexpr size_t HX_CONSOLE_LINE_MAX = 128;

static char g_console_line[HX_CONSOLE_LINE_MAX];
static size_t g_console_len = 0;
static bool g_console_overflow = false;
static bool g_last_was_cr = false;

static void ConsolePrompt() {
  LogSinkWriteRaw("hx> ");
}

void ConsoleShowPrompt() {
  ConsolePrompt();
}

static void ConsoleClearLine() {
  memset(g_console_line, 0, sizeof(g_console_line));
  g_console_len = 0;
  g_console_overflow = false;
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

static void ConsoleExecuteCommand(const char* line) {
  if (!line || !line[0]) {
    ConsolePrompt();
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

  LogWarn("CON: unknown command: %s", line);
  ConsolePrompt();
}

static void ConsoleHandleLine() {
  if (g_console_overflow) {
    LogWarn("CON: input too long");
    ConsoleClearLine();
    ConsolePrompt();
    return;
  }

  g_console_line[g_console_len] = '\0';

  size_t start = 0;
  while ((start < g_console_len) &&
         ((g_console_line[start] == ' ') || (g_console_line[start] == '\t'))) {
    start++;
  }

  size_t end = g_console_len;
  while ((end > start) &&
         ((g_console_line[end - 1] == ' ') || (g_console_line[end - 1] == '\t'))) {
    end--;
  }

  g_console_line[end] = '\0';
  ConsoleExecuteCommand(&g_console_line[start]);
  ConsoleClearLine();
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

      LogSinkWriteRaw("\r\n");
      ConsoleHandleLine();
      continue;
    }

    if (ch == '\r') {
      g_last_was_cr = true;
      LogSinkWriteRaw("\r\n");
      ConsoleHandleLine();
      continue;
    }

    g_last_was_cr = false;

    if ((ch == 0x08) || (ch == 0x7F)) {
      if (!g_console_overflow && (g_console_len > 0)) {
        g_console_len--;
        g_console_line[g_console_len] = '\0';
        LogSinkWriteRaw("\b \b");
      }
      continue;
    }

    if ((ch < 32) || (ch > 126)) {
      continue;
    }

    if (g_console_overflow) {
      continue;
    }

    if (g_console_len >= (HX_CONSOLE_LINE_MAX - 1)) {
      g_console_overflow = true;
      continue;
    }

    g_console_line[g_console_len++] = (char)ch;
    LogSinkWriteChar((char)ch);
  }
}

static bool ConsoleInit() {
  ConsoleClearLine();
  LogInfo("CON: init");
  return true;
}

static void ConsoleStart() {
  LogInfo("CON: start");
  LogInfo("CON: commands available: reboot, log, logclr, logstat");
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