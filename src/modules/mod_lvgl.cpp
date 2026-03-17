#include "hexaos.h"

static bool LvglInit() {
  LogInfo("LVG: init");
  return true;
}

static void LvglStart() {
  LogInfo("LVG: start");
}

static void LvglLoop() {
}

static void LvglEvery100ms() {
}

static void LvglEverySecond() {
}

const HxModule ModuleLvgl = {
  .name = "lvgl",
  .init = LvglInit,
  .start = LvglStart,
  .loop = LvglLoop,
  .every_100ms = LvglEvery100ms,
  .every_1s = LvglEverySecond
};