/*
  HexaOS - hexaos.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Primary public system header for HexaOS.
  Aggregates common includes, shared runtime structures and the public prototypes used across core, services, platform adapters and modules.
*/

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "headers/hx_build.h"
#include "headers/hx_config.h"

// SYSTEM CORE HEADERS
#include "system/core/boot.h"
#include "system/core/log.h"
#include "system/core/runtime.h"
#include "system/core/module_registry.h"



// core


void Panic(const char* reason);

// services
bool FactoryDataInit();

bool StateInit();
bool StateLoad();
bool StateSave();
bool StateGetBool(const char* key, bool defval);
int32_t StateGetInt(const char* key, int32_t defval);
bool StateSetBool(const char* key, bool value);
bool StateSetInt(const char* key, int32_t value);




// module system
void ModuleInitAll();
void ModuleStartAll();
void ModuleLoopAll();
void ModuleEvery100ms();
void ModuleEverySecond();