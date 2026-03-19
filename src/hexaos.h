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

// Build variables
#include "headers/include/pre_build.h"
#include "headers/hx_build.h"
#include "headers/include/pos_build.h"
#include "headers/hx_config.h"

// SYSTEM CORE HEADERS
#include "system/core/panic.h"
#include "system/core/boot.h"
#include "system/core/log.h"
#include "system/core/runtime.h"
#include "system/core/module_registry.h"