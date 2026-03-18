#include "hexaos.h"
#include "headers/hx_platform_nvs.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static_assert(sizeof(HX_BUILD_DEFAULT_DEVICE_NAME) <= (HX_SETUP_DEVICE_NAME_MAX + 1),
              "HX_BUILD_DEFAULT_DEVICE_NAME is too long");

static bool g_setup_ready = false;

HxSetup HxSetupData = {};
const HxSetup HxSetupDefaults = {
  HX_BUILD_DEFAULT_DEVICE_NAME,
  (HxLogLevel)HX_BUILD_DEFAULT_LOG_LEVEL,
  (HX_BUILD_DEFAULT_SAFEBOOT_ENABLE != 0)
};

static bool SetupLogLevelIsValid(HxLogLevel level) {
  return (level >= HX_LOG_ERROR) && (level <= HX_LOG_DEBUG);
}

static bool SetupStoreBoolOverride(const char* key, bool value, bool defval) {
  if (value == defval) {
    return HxNvsEraseKey(HX_NVS_STORE_CONFIG, key);
  }

  return HxNvsSetBool(HX_NVS_STORE_CONFIG, key, value);
}

static bool SetupStoreIntOverride(const char* key, int32_t value, int32_t defval) {
  if (value == defval) {
    return HxNvsEraseKey(HX_NVS_STORE_CONFIG, key);
  }

  return HxNvsSetInt(HX_NVS_STORE_CONFIG, key, value);
}

static bool SetupStoreStringOverride(const char* key, const char* value, const char* defval) {
  const char* current = value ? value : "";
  const char* defaults = defval ? defval : "";

  if (strcmp(current, defaults) == 0) {
    return HxNvsEraseKey(HX_NVS_STORE_CONFIG, key);
  }

  return HxNvsSetString(HX_NVS_STORE_CONFIG, key, current);
}

const char* SetupLogLevelText(HxLogLevel level) {
  switch (level) {
    case HX_LOG_ERROR: return "error";
    case HX_LOG_WARN:  return "warn";
    case HX_LOG_INFO:  return "info";
    case HX_LOG_DEBUG: return "debug";
    default:           return "unknown";
  }
}

bool SetupParseLogLevel(const char* text, HxLogLevel* level) {
  if (!text || !text[0] || !level) {
    return false;
  }

  if ((strcasecmp(text, "error") == 0) || (strcasecmp(text, "err") == 0)) {
    *level = HX_LOG_ERROR;
    return true;
  }

  if ((strcasecmp(text, "warn") == 0) || (strcasecmp(text, "warning") == 0) || (strcasecmp(text, "wrn") == 0)) {
    *level = HX_LOG_WARN;
    return true;
  }

  if ((strcasecmp(text, "info") == 0) || (strcasecmp(text, "inf") == 0)) {
    *level = HX_LOG_INFO;
    return true;
  }

  if ((strcasecmp(text, "debug") == 0) || (strcasecmp(text, "dbg") == 0)) {
    *level = HX_LOG_DEBUG;
    return true;
  }

  char* endptr = nullptr;
  long raw = strtol(text, &endptr, 10);
  if (endptr && (*endptr == '\0') && (raw >= (long)HX_LOG_ERROR) && (raw <= (long)HX_LOG_DEBUG)) {
    *level = (HxLogLevel)raw;
    return true;
  }

  return false;
}

void SetupResetToDefaults(HxSetup* setup) {
  if (!setup) {
    return;
  }

  *setup = HxSetupDefaults;
}

bool SetupSetDeviceName(const char* value) {
  if (!value) {
    return false;
  }

  size_t len = strlen(value);
  if ((len == 0) || (len > HX_SETUP_DEVICE_NAME_MAX)) {
    return false;
  }

  memset(HxSetupData.device_name, 0, sizeof(HxSetupData.device_name));
  memcpy(HxSetupData.device_name, value, len);
  HxSetupData.device_name[len] = '\0';
  return true;
}

bool SetupSetLogLevel(HxLogLevel value) {
  if (!SetupLogLevelIsValid(value)) {
    return false;
  }

  HxSetupData.log_level = value;
  return true;
}

bool SetupSetSafebootEnable(bool value) {
  HxSetupData.safeboot_enable = value;
  return true;
}

bool SetupInit() {
  SetupResetToDefaults(&HxSetupData);
  g_setup_ready = EspNvsOpenConfig();
  return g_setup_ready;
}

bool SetupLoad() {
  SetupResetToDefaults(&HxSetupData);

  if (!g_setup_ready) {
    Hx.config_loaded = false;
    return false;
  }

  String text_value;
  if (HxNvsGetString(HX_NVS_STORE_CONFIG, HX_CFG_DEVICE_NAME, text_value)) {
    SetupSetDeviceName(text_value.c_str());
  }

  int32_t int_value = 0;
  if (HxNvsGetInt(HX_NVS_STORE_CONFIG, HX_CFG_LOG_LEVEL, &int_value)) {
    HxLogLevel level = (HxLogLevel)int_value;
    if (SetupLogLevelIsValid(level)) {
      HxSetupData.log_level = level;
    }
  }

  bool bool_value = false;
  if (HxNvsGetBool(HX_NVS_STORE_CONFIG, HX_CFG_SAFEBOOT_ENABLE, &bool_value)) {
    HxSetupData.safeboot_enable = bool_value;
  }

  Hx.config_loaded = true;
  return true;
}

bool SetupSave() {
  if (!g_setup_ready) {
    return false;
  }

  if (!SetupStoreStringOverride(HX_CFG_DEVICE_NAME,
                                HxSetupData.device_name,
                                HxSetupDefaults.device_name)) {
    return false;
  }

  if (!SetupStoreIntOverride(HX_CFG_LOG_LEVEL,
                             (int32_t)HxSetupData.log_level,
                             (int32_t)HxSetupDefaults.log_level)) {
    return false;
  }

  if (!SetupStoreBoolOverride(HX_CFG_SAFEBOOT_ENABLE,
                              HxSetupData.safeboot_enable,
                              HxSetupDefaults.safeboot_enable)) {
    return false;
  }

  if (!HxNvsCommit(HX_NVS_STORE_CONFIG)) {
    return false;
  }

  Hx.config_loaded = true;
  return true;
}

void SetupApply() {
  LogSetLevel(HxSetupData.log_level);
  Hx.safeboot = HxSetupData.safeboot_enable;
}