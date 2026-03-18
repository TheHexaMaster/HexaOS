/*
  HexaOS - hx_setup.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Defines the HexaOS runtime setup schema stored in RAM and synchronized
  with persistent configuration storage. This header provides the typed
  setup structure, default setup access, and the public API for loading,
  saving, resetting, applying, and updating runtime configuration values.

  Use case
  Used as the central runtime configuration contract for system-wide setup
  values such as device identity, log level, safeboot behavior, and other
  configurable options that are loaded from NVS and applied during boot
  or updated later from the console or other management interfaces.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hx_build.h"
#include "hx_types.h"

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

bool SetupSetDeviceName(const char* value);
bool SetupSetLogLevel(HxLogLevel value);
bool SetupSetSafebootEnable(bool value);

const char* SetupLogLevelText(HxLogLevel level);
bool SetupParseLogLevel(const char* text, HxLogLevel* level);