/*
  HexaOS - nvs_store.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared NVS persistence backend used by core config and state services.
  Exposes backend-oriented open, read, write, commit, format and statistics
  helpers while keeping storage policy and schema logic outside of the NVS
  layer.
*/

#pragma once

#include <WString.h>

#include "system/adapters/nvs_adapter.h"

using HxNvsStoreReadResult = HxNvsReadResult;
using HxNvsStoreStats = HxNvsStats;

bool NvsStoreOpen(HxNvsStore store);

HxNvsStoreReadResult NvsStoreReadBool(HxNvsStore store, const char* key, bool* value);
HxNvsStoreReadResult NvsStoreReadInt(HxNvsStore store, const char* key, int32_t* value);
HxNvsStoreReadResult NvsStoreReadFloat(HxNvsStore store, const char* key, float* value);
HxNvsStoreReadResult NvsStoreReadString(HxNvsStore store, const char* key, String& value);

bool NvsStoreGetBool(HxNvsStore store, const char* key, bool* value);
bool NvsStoreGetInt(HxNvsStore store, const char* key, int32_t* value);
bool NvsStoreGetFloat(HxNvsStore store, const char* key, float* value);
bool NvsStoreGetString(HxNvsStore store, const char* key, String& value);

bool NvsStoreWriteBool(HxNvsStore store, const char* key, bool value);
bool NvsStoreWriteInt(HxNvsStore store, const char* key, int32_t value);
bool NvsStoreWriteFloat(HxNvsStore store, const char* key, float value);
bool NvsStoreWriteString(HxNvsStore store, const char* key, const char* value);
bool NvsStoreEraseKey(HxNvsStore store, const char* key);

bool NvsStoreCommit(HxNvsStore store);
bool NvsStoreGetStats(HxNvsStore store, HxNvsStoreStats* out_stats);
bool NvsStoreFormat(HxNvsStore store);
