#include "hexaos.h"

static bool ConsoleInit() {
  LogInfo("CON: init");
  return true;
}

static void ConsoleStart() {
  LogInfo("CON: start");
}

static void ConsoleLoop() {
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