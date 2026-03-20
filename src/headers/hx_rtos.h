/*
  HexaOS - hx_rtos.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Public RTOS service API for HexaOS.
  Exposes the central synchronization and task-yielding primitives that the rest of the system should use instead of calling the underlying FreeRTOS backend directly.
*/

#pragma once

#include "system/handlers/rtos_handler.h"
