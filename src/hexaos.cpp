#include "hexaos.h"

void setup() {
  BootInit();
  LogInit();

  BootPrintBanner();
  BootPrintResetInfo();
  EspPrintChipInfo();

  if (!EspNvsInit()) {
    Panic("NVS init failed");
  }

  if (!FactoryDataInit()) {
    LogWarn("FACT: init failed");
  }

  if (!SetupInit()) {
    LogWarn("SETUP: init failed");
  }

  if (!SetupLoad()) {
    LogWarn("SETUP: load failed");
  }

  SetupApply();

  if (!StateInit()) {
    LogWarn("STA: init failed");
  }

  if (!FilesInit()) {
    LogWarn("FIL: init failed");
  }

  if (!StateLoad()) {
    LogWarn("STA: load failed");
  }

  if (!FilesMount()) {
    LogWarn("FIL: mount failed");
  }

  ModuleInitAll();
  ModuleStartAll();

#if HX_ENABLE_MODULE_CONSOLE
  ConsoleShowPrompt();
#endif
}

void loop() {
  static uint32_t last_100ms = 0;
  static uint32_t last_1s = 0;

  uint32_t now = millis();
  Hx.uptime_ms = now;

  ModuleLoopAll();

  if ((now - last_100ms) >= 100) {
    last_100ms = now;
    ModuleEvery100ms();
  }

  if ((now - last_1s) >= 1000) {
    last_1s = now;
    ModuleEverySecond();
  }
}