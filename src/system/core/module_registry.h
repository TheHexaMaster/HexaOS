/*
  HexaOS - module_registry.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  HexaOS module interface contract.
  Declares the lightweight module descriptor used by the core registry so optional subsystems can expose init, loop and timed lifecycle hooks in a uniform way.
*/

#pragma once

typedef struct {
  const char* name;
  bool (*init)();
  void (*start)();
  void (*loop)();
  void (*every_100ms)();
  void (*every_1s)();
} HxModule;

extern const HxModule ModuleSystem;

#if HX_ENABLE_MODULE_CONSOLE
  extern const HxModule ModuleConsole;
#endif

extern const HxModule ModuleStorage;
extern const HxModule ModuleBerry;
extern const HxModule ModuleWeb;
extern const HxModule ModuleLvgl;


void ModuleInitAll();
void ModuleStartAll();
void ModuleLoopAll();
void ModuleEvery100ms();
void ModuleEverySecond();