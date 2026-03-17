#include "hexaos.h"
#include <esp_system.h>
#include <string.h>

static constexpr size_t HX_CONSOLE_LINE_MAX = 128;

static char g_console_line[HX_CONSOLE_LINE_MAX];
static size_t g_console_len = 0;
static bool g_console_overflow = false;
static bool g_last_was_cr = false;

static void ConsolePrompt() {
  Serial.print("hx> ");
}

static void ConsoleClearLine() {
  memset(g_console_line, 0, sizeof(g_console_line));
  g_console_len = 0;
  g_console_overflow = false;
}

static void ConsoleExecuteCommand(const char* line) {
  if (!line || !line[0]) {
    ConsolePrompt();
    return;
  }

  if (strcmp(line, "reboot") == 0) {
    LogWarn("CON: soft restart requested");
    Serial.flush();
    delay(100);
    esp_restart();
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

      Serial.println();
      ConsoleHandleLine();
      continue;
    }

    if (ch == '\r') {
      g_last_was_cr = true;
      Serial.println();
      ConsoleHandleLine();
      continue;
    }

    g_last_was_cr = false;

    if ((ch == 0x08) || (ch == 0x7F)) {
      if (!g_console_overflow && (g_console_len > 0)) {
        g_console_len--;
        g_console_line[g_console_len] = '\0';
        Serial.print("\b \b");
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
    Serial.write((char)ch);
  }
}

static bool ConsoleInit() {
  ConsoleClearLine();
  LogInfo("CON: init");
  return true;
}

static void ConsoleStart() {
  LogInfo("CON: start");
  LogInfo("CON: command available: reboot");
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
