/*
  HexaOS - mod_i2c.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  I2C lifecycle module for HexaOS.
  Delegates bus initialization and initial device scan to the I2C handler.
  The handler discovers which buses are wired in the board pinmap, initializes
  them, and performs an address scan on each ready bus at startup.

  Future revisions will add per-driver scheduling via the timed callbacks.
*/

#include "system/core/module_registry.h"
#include "system/core/log.h"
#include "system/handlers/i2c_handler.h"

static constexpr const char* TAG = "I2C";

static bool I2cInit() {
  if (!I2cHandlerInit()) {
    HX_LOGW(TAG, "no I2C buses available");
    return false;
  }
  return true;
}

const HxModule ModuleI2c = {
  .name        = "i2c",
  .init        = I2cInit,
  .start       = nullptr,
  .loop        = nullptr,
  .every_10ms  = nullptr,
  .every_100ms = nullptr,
  .every_1s    = nullptr
};
