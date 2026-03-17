#pragma once

#include <Arduino.h>
#include <stdint.h>

enum HxNvsStore : uint8_t {
  HX_NVS_STORE_CONFIG = 0,
  HX_NVS_STORE_STATE = 1,
  HX_NVS_STORE_FACTORY = 2
};

bool HxNvsGetBool(HxNvsStore store, const char* key, bool* value);
bool HxNvsGetInt(HxNvsStore store, const char* key, int32_t* value);
bool HxNvsGetString(HxNvsStore store, const char* key, String& value);

bool HxNvsSetBool(HxNvsStore store, const char* key, bool value);
bool HxNvsSetInt(HxNvsStore store, const char* key, int32_t value);
bool HxNvsSetString(HxNvsStore store, const char* key, const char* value);

bool HxNvsCommit(HxNvsStore store);
