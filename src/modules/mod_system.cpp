#include "hexaos.h"

static bool SystemInit() {
  LogInfo("SYS: init");
  return true;
}

static void SystemStart() {
  LogInfo("SYS: start");
}

static void SystemLoop() {
}

static void SystemEvery100ms() {
}

static void SystemEverySecond() {
  LogInfo("SYS: uptime=%lu ms boot_count=%lu",
          (unsigned long)Hx.uptime_ms,
          (unsigned long)Hx.boot_count);
}

const HxModule ModuleSystem = {
  .name = "system",
  .init = SystemInit,
  .start = SystemStart,
  .loop = SystemLoop,
  .every_100ms = SystemEvery100ms,
  .every_1s = SystemEverySecond
};