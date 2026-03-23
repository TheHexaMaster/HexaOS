/*
  HexaOS - mod_spi.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  SPI lifecycle module for HexaOS.
  Delegates bus initialization to the SPI handler. The handler discovers
  which buses are wired in the board pinmap and initializes them. Drivers
  register their devices on the already-initialized buses via the handler.
*/

#include "system/core/module_registry.h"
#include "system/core/log.h"
#include "system/handlers/spi_handler.h"

static constexpr const char* TAG = "SPI";

static bool SpiInit() {
  if (!SpiHandlerInit()) {
    HX_LOGW(TAG, "no SPI buses available");
    return false;
  }
  return true;
}

const HxModule ModuleSpi = {
  .name        = "spi",
  .init        = SpiInit,
  .start       = nullptr,
  .loop        = nullptr,
  .every_10ms  = nullptr,
  .every_100ms = nullptr,
  .every_1s    = nullptr
};
