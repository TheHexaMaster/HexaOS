/*
  HexaOS - sdmmc_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  SDMMC hardware adapter implementation.
  Reads SDMMC pin assignments from the board pinmap and configures the
  SDMMC bus peripheral. Full driver integration is pending.
*/

#include "sdmmc_adapter.h"

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_SD

#include "headers/hx_pinfunc.h"
#include "system/core/log.h"
#include "system/core/pinmap.h"

bool SdmmcInit() {
  int clk = PinmapGetGpioForFunction(HX_PIN_SDMMC0_CLK);
  int cmd = PinmapGetGpioForFunction(HX_PIN_SDMMC0_CMD);
  int d0  = PinmapGetGpioForFunction(HX_PIN_SDMMC0_D0);

  if (clk < 0 || cmd < 0 || d0 < 0) {
    LogError("SDMMC: mandatory pins not mapped (clk=%d cmd=%d d0=%d)", clk, cmd, d0);
    return false;
  }

  // TODO: configure SDMMC peripheral with mapped pins
  LogWarn("SDMMC: hardware init not implemented (stub)");
  return false;
}

bool SdmmcDeinit() {
  return true;
}

#endif // HX_ENABLE_FEATURE_SD
