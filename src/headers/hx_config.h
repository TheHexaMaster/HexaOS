/*
  HexaOS - hx_config.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Defines the HexaOS runtime config structure stored in RAM together with
  the public lifecycle API used to initialize, load, save, reset and apply
  configuration. Per-key schema metadata and generic config item access are
  intentionally defined outside this header to keep the config contract small
  and stable.
*/

#pragma once

static constexpr size_t HX_CONFIG_DEVICE_NAME_MAX = 32;

struct HxConfig {
  char device_name[HX_CONFIG_DEVICE_NAME_MAX + 1];
  HxLogLevel log_level;
  bool safeboot_enable;
};

extern HxConfig HxConfigData;
extern const HxConfig HxConfigDefaults;

void ConfigResetToDefaults(HxConfig* config);

bool ConfigInit();
bool ConfigLoad();
bool ConfigSave();
void ConfigApply();

const char* ConfigLogLevelText(HxLogLevel level);
bool ConfigParseLogLevel(const char* text, HxLogLevel* level);