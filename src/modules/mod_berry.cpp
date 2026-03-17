#include "hexaos.h"

static bool BerryInit() {
  LogInfo("BRY: init");
  return true;
}

static void BerryStart() {
  LogInfo("BRY: start");
}

static void BerryLoop() {
}

static void BerryEvery100ms() {
}

static void BerryEverySecond() {
}

const HxModule ModuleBerry = {
  .name = "berry",
  .init = BerryInit,
  .start = BerryStart,
  .loop = BerryLoop,
  .every_100ms = BerryEvery100ms,
  .every_1s = BerryEverySecond
};