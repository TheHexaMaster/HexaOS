/*
  HexaOS - sdmmc_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  SDMMC hardware adapter for HexaOS SD card support.
  Handles low-level SDMMC bus configuration using GPIO assignments from the
  board pinmap. Consumed by fatfs_adapter during mount.
  Gated by HX_ENABLE_FEATURE_SD.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_SD

// Initialize the SDMMC bus. Reads pin assignments from the board pinmap
// (HX_PIN_SDMMC0_CLK, HX_PIN_SDMMC0_CMD, HX_PIN_SDMMC0_D0..D3).
// Returns false if mandatory pins are not mapped or hardware init fails.
bool SdmmcInit();
bool SdmmcDeinit();

#endif // HX_ENABLE_FEATURE_SD
