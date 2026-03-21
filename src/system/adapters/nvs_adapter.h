/*
  HexaOS - nvs_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Unified NVS adapter for HexaOS configuration and runtime state persistence.
  Provides partition-aware open, read, write, erase, commit, statistics and
  format helpers for the dedicated config and state NVS stores.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <WString.h>

enum HxNvsStore : uint8_t {
  HX_NVS_STORE_CONFIG = 0,
  HX_NVS_STORE_STATE = 1
};

enum HxNvsReadResult : uint8_t {
  HX_NVS_READ_OK = 0,
  HX_NVS_READ_NOT_FOUND = 1,
  HX_NVS_READ_ERROR = 2
};

struct HxNvsStats {
  const char* partition_label;
  const char* namespace_name;
  size_t used_entries;
  size_t free_entries;
  size_t available_entries;
  size_t total_entries;
  size_t namespace_entries;
};

bool HxNvsOpen(HxNvsStore store);
bool HxNvsOpenConfig();
bool HxNvsOpenState();

HxNvsReadResult HxNvsReadBool(HxNvsStore store, const char* key, bool* value);
HxNvsReadResult HxNvsReadInt(HxNvsStore store, const char* key, int32_t* value);
HxNvsReadResult HxNvsReadFloat(HxNvsStore store, const char* key, float* value);
HxNvsReadResult HxNvsReadString(HxNvsStore store, const char* key, String& value);

bool HxNvsGetBool(HxNvsStore store, const char* key, bool* value);
bool HxNvsGetInt(HxNvsStore store, const char* key, int32_t* value);
bool HxNvsGetFloat(HxNvsStore store, const char* key, float* value);
bool HxNvsGetString(HxNvsStore store, const char* key, String& value);

bool HxNvsSetBool(HxNvsStore store, const char* key, bool value);
bool HxNvsSetInt(HxNvsStore store, const char* key, int32_t value);
bool HxNvsSetFloat(HxNvsStore store, const char* key, float value);
bool HxNvsSetString(HxNvsStore store, const char* key, const char* value);
bool HxNvsEraseKey(HxNvsStore store, const char* key);

bool HxNvsCommit(HxNvsStore store);
bool HxNvsGetStats(HxNvsStore store, HxNvsStats* out_stats);
bool HxNvsFormat(HxNvsStore store);
