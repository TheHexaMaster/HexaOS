/*
  HexaOS - module_registry.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  HexaOS module interface contract and runtime registry.
  Declares the module descriptor used by the core lifecycle dispatcher and
  the runtime record used for introspection. Each compiled-in module exposes
  init, start, loop and timed lifecycle hooks through a uniform descriptor.
  The registry tracks per-module init and start outcomes for runtime queries.
*/

#pragma once

#include <stddef.h>

#include "headers/hx_build.h"

// ---------------------------------------------------------------------------
// Module descriptor — defines the lifecycle contract for one domain module
// ---------------------------------------------------------------------------

typedef struct {
  const char* name;
  bool (*init)();
  void (*start)();
  void (*loop)();
  void (*every_10ms)();
  void (*every_100ms)();
  void (*every_1s)();
} HxModule;

// ---------------------------------------------------------------------------
// Runtime record — populated by the registry during lifecycle dispatch.
// Used for introspection only. Not persisted.
// ---------------------------------------------------------------------------

typedef struct {
  const char* name;
  bool        ready;    // true if init() succeeded (or module has no init callback)
  bool        started;  // true if start() was called
} HxModuleRecord;

// ---------------------------------------------------------------------------
// Module instances (defined in mod_*.cpp)
// ---------------------------------------------------------------------------

#if HX_ENABLE_MODULE_STORAGE
extern const HxModule ModuleStorage;
#endif
#if HX_ENABLE_MODULE_I2C
extern const HxModule ModuleI2c;
#endif
#if HX_ENABLE_MODULE_SPI
extern const HxModule ModuleSpi;
#endif
#if HX_ENABLE_MODULE_UART
extern const HxModule ModuleUart;
#endif
#if HX_ENABLE_MODULE_BERRY
extern const HxModule ModuleBerry;
#endif
#if HX_ENABLE_MODULE_WEB
extern const HxModule ModuleWeb;
#endif
#if HX_ENABLE_MODULE_LVGL
extern const HxModule ModuleLvgl;
#endif

// ---------------------------------------------------------------------------
// Lifecycle dispatch
// ---------------------------------------------------------------------------

void ModuleInitAll();
void ModuleStartAll();
void ModuleLoopAll();
void ModuleEvery10ms();
void ModuleEvery100ms();
void ModuleEverySecond();

// ---------------------------------------------------------------------------
// Introspection API
// ---------------------------------------------------------------------------

// Returns the number of modules compiled into this build.
size_t ModuleRegisteredCount();

// Returns the runtime record for the module at the given index,
// or nullptr if the index is out of range.
const HxModuleRecord* ModuleRecordAt(size_t index);
