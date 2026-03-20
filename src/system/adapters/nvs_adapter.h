/*
  HexaOS - nvs_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Platform abstraction for NVS storage access.
  Declares the low-level read, write and commit helpers used by higher-level services to persist configuration and runtime state.
*/

#pragma once

#include <Arduino.h>
#include <stdint.h>

enum HxNvsStore : uint8_t {
  HX_NVS_STORE_CONFIG = 0,
  HX_NVS_STORE_STATE = 1
};


bool EspNvsOpenConfig();
bool EspNvsOpenState();


struct HxNvsStats {
  const char* partition_label;
  const char* namespace_name;
  size_t used_entries;
  size_t free_entries;
  size_t available_entries;
  size_t total_entries;
  size_t namespace_entries;
};

bool HxNvsGetBool(HxNvsStore store, const char* key, bool* value);
bool HxNvsGetInt(HxNvsStore store, const char* key, int32_t* value);
bool HxNvsGetString(HxNvsStore store, const char* key, String& value);

bool HxNvsSetBool(HxNvsStore store, const char* key, bool value);
bool HxNvsSetInt(HxNvsStore store, const char* key, int32_t value);
bool HxNvsSetString(HxNvsStore store, const char* key, const char* value);
bool HxNvsEraseKey(HxNvsStore store, const char* key);

bool HxNvsCommit(HxNvsStore store);
bool HxNvsGetStats(HxNvsStore store, HxNvsStats* out_stats);
bool HxNvsFormat(HxNvsStore store);