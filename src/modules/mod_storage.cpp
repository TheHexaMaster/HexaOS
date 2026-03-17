#include "hexaos.h"

static bool StorageInit() {
  LogInfo("STO: init");
  return true;
}

static void StorageStart() {
  LogInfo("STO: start");
}

static void StorageLoop() {
}

static void StorageEvery100ms() {
}

static void StorageEverySecond() {
}

const HxModule ModuleStorage = {
  .name = "storage",
  .init = StorageInit,
  .start = StorageStart,
  .loop = StorageLoop,
  .every_100ms = StorageEvery100ms,
  .every_1s = StorageEverySecond
};