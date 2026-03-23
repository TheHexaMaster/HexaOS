/*
  HexaOS - module_registry.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Static module registry and lifecycle dispatcher.
  Owns the ordered list of compiled HexaOS modules and fans out init, start,
  loop and periodic callbacks to each registered optional module. Maintains
  per-module runtime records for introspection queries.
*/

#include "module_registry.h"

#include <stddef.h>

#include "headers/hx_build.h"
#include "system/core/log.h"

// ---------------------------------------------------------------------------
// Compiled module table
// ---------------------------------------------------------------------------

static const HxModule* kModules[] = {
#if HX_ENABLE_MODULE_STORAGE
  &ModuleStorage,
#endif
#if HX_ENABLE_MODULE_NETWORK
  &ModuleNetwork,
#endif
#if HX_ENABLE_MODULE_I2C
  &ModuleI2c,
#endif
#if HX_ENABLE_MODULE_SPI
  &ModuleSpi,
#endif
#if HX_ENABLE_MODULE_UART
  &ModuleUart,
#endif
#if HX_ENABLE_MODULE_BERRY
  &ModuleBerry,
#endif
#if HX_ENABLE_MODULE_WEB
  &ModuleWeb,
#endif
#if HX_ENABLE_MODULE_LVGL
  &ModuleLvgl,
#endif
  nullptr
};

static constexpr size_t kModuleCount = sizeof(kModules) / sizeof(kModules[0]);

// ---------------------------------------------------------------------------
// Runtime records — populated during lifecycle dispatch, used for introspection
// ---------------------------------------------------------------------------

static HxModuleRecord g_module_records[kModuleCount] = {};
static bool           g_module_ready[kModuleCount]   = { false };

// ---------------------------------------------------------------------------
// Lifecycle dispatch
// ---------------------------------------------------------------------------

void ModuleInitAll() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod) {
      continue;
    }

    g_module_records[i].name    = mod->name;
    g_module_records[i].started = false;

    if (!mod->init) {
      g_module_ready[i]              = true;
      g_module_records[i].ready      = true;
      continue;
    }

    HX_LOGI("MOD", "init %s", mod->name);
    g_module_ready[i]         = mod->init();
    g_module_records[i].ready = g_module_ready[i];

    if (!g_module_ready[i]) {
      HX_LOGW("MOD", "init failed %s", mod->name);
    }
  }
}

void ModuleStartAll() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod || !g_module_ready[i] || !mod->start) {
      continue;
    }

    HX_LOGI("MOD", "start %s", mod->name);
    mod->start();
    g_module_records[i].started = true;
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

void ModuleEvery10ms() {
  for (size_t i = 0; i < kModuleCount; i++) {
    const HxModule* mod = kModules[i];
    if (!mod || !g_module_ready[i] || !mod->every_10ms) {
      continue;
    }

    mod->every_10ms();
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

// ---------------------------------------------------------------------------
// Introspection
// ---------------------------------------------------------------------------

size_t ModuleRegisteredCount() {
  size_t count = 0;
  for (size_t i = 0; i < kModuleCount; i++) {
    if (kModules[i]) {
      count++;
    }
  }
  return count;
}

const HxModuleRecord* ModuleRecordAt(size_t index) {
  size_t seen = 0;
  for (size_t i = 0; i < kModuleCount; i++) {
    if (!kModules[i]) {
      continue;
    }
    if (seen == index) {
      return &g_module_records[i];
    }
    seen++;
  }
  return nullptr;
}
