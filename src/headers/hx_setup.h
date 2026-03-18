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