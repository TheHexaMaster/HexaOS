/*
  HexaOS - hx_setup.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Defines the HexaOS runtime setup structure stored in RAM together with
  the public lifecycle API used to initialize, load, save, reset and apply
  configuration. Per-key schema metadata and generic config item access are
  intentionally defined outside this header to keep the setup contract small
  and stable.
*/

#pragma once

static constexpr size_t HX_SETUP_DEVICE_NAME_MAX = 32;

struct HxSetup {
  char device_name[HX_SETUP_DEVICE_NAME_MAX + 1];
  HxLogLevel log_level;
  bool safeboot_enable;
};

extern HxSetup HxSetupData;
extern const HxSetup HxSetupDefaults;

void SetupResetToDefaults(HxSetup* setup);

bool SetupInit();
bool SetupLoad();
bool SetupSave();
void SetupApply();

const char* SetupLogLevelText(HxLogLevel level);
bool SetupParseLogLevel(const char* text, HxLogLevel* level);