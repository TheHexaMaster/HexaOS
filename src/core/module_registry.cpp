#include "hexaos.h"

static const HxModule* kModules[] = {
  &ModuleSystem,
  &ModuleConsole,
  &ModuleStorage,
  &ModuleBerry,
  &ModuleWeb,
  &ModuleLvgl,
  nullptr
};

static constexpr size_t kModuleCount = (sizeof(kModules) / sizeof(kModules[0])) - 1;
static bool g_module_ready[kModuleCount] = { false };

void ModuleInitAll() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod) {
      continue;
    }

    if (!mod->init) {
      g_module_ready[i] = true;
      continue;
    }

    LogInfo("MOD: init %s", mod->name);
    g_module_ready[i] = mod->init();

    if (!g_module_ready[i]) {
      LogWarn("MOD: init failed %s", mod->name);
    }
  }
}

void ModuleStartAll() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod || !g_module_ready[i] || !mod->start) {
      continue;
    }

    LogInfo("MOD: start %s", mod->name);
    mod->start();
  }
}

void ModuleLoopAll() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod || !g_module_ready[i] || !mod->loop) {
      continue;
    }

    mod->loop();
  }
}

void ModuleEvery100ms() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod || !g_module_ready[i] || !mod->every_100ms) {
      continue;
    }

    mod->every_100ms();
  }
}

void ModuleEverySecond() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod || !g_module_ready[i] || !mod->every_1s) {
      continue;
    }

    mod->every_1s();
  }
}
