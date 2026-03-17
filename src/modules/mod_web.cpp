#include "hexaos.h"

static bool WebInit() {
  LogInfo("WEB: init");
  return true;
}

static void WebStart() {
  LogInfo("WEB: start");
}

static void WebLoop() {
}

static void WebEvery100ms() {
}

static void WebEverySecond() {
}

const HxModule ModuleWeb = {
  .name = "web",
  .init = WebInit,
  .start = WebStart,
  .loop = WebLoop,
  .every_100ms = WebEvery100ms,
  .every_1s = WebEverySecond
};