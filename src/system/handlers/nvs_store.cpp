/*
  HexaOS - nvs_store.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared NVS persistence backend implementation used by core config and state
  services. This layer centralizes NVS backend selection and typed storage
  access while remaining intentionally unaware of config and state business
  rules.
*/

#include "nvs_store.h"

bool NvsStoreOpen(HxNvsStore store) {
  switch (store) {
    case HX_NVS_STORE_CONFIG:
      return EspNvsOpenConfig();

    case HX_NVS_STORE_STATE:
      return EspNvsOpenState();

    default:
      return false;
  }
}

HxNvsStoreReadResult NvsStoreReadBool(HxNvsStore store, const char* key, bool* value) {
  return HxNvsReadBool(store, key, value);
}

HxNvsStoreReadResult NvsStoreReadInt(HxNvsStore store, const char* key, int32_t* value) {
  return HxNvsReadInt(store, key, value);
}

HxNvsStoreReadResult NvsStoreReadFloat(HxNvsStore store, const char* key, float* value) {
  return HxNvsReadFloat(store, key, value);
}

HxNvsStoreReadResult NvsStoreReadString(HxNvsStore store, const char* key, String& value) {
  return HxNvsReadString(store, key, value);
}

bool NvsStoreGetBool(HxNvsStore store, const char* key, bool* value) {
  return HxNvsGetBool(store, key, value);
}

bool NvsStoreGetInt(HxNvsStore store, const char* key, int32_t* value) {
  return HxNvsGetInt(store, key, value);
}

bool NvsStoreGetFloat(HxNvsStore store, const char* key, float* value) {
  return HxNvsGetFloat(store, key, value);
}

bool NvsStoreGetString(HxNvsStore store, const char* key, String& value) {
  return HxNvsGetString(store, key, value);
}

bool NvsStoreWriteBool(HxNvsStore store, const char* key, bool value) {
  return HxNvsSetBool(store, key, value);
}

bool NvsStoreWriteInt(HxNvsStore store, const char* key, int32_t value) {
  return HxNvsSetInt(store, key, value);
}

bool NvsStoreWriteFloat(HxNvsStore store, const char* key, float value) {
  return HxNvsSetFloat(store, key, value);
}

bool NvsStoreWriteString(HxNvsStore store, const char* key, const char* value) {
  return HxNvsSetString(store, key, value);
}

bool NvsStoreEraseKey(HxNvsStore store, const char* key) {
  return HxNvsEraseKey(store, key);
}

bool NvsStoreCommit(HxNvsStore store) {
  return HxNvsCommit(store);
}

bool NvsStoreGetStats(HxNvsStore store, HxNvsStoreStats* out_stats) {
  return HxNvsGetStats(store, out_stats);
}

bool NvsStoreFormat(HxNvsStore store) {
  return HxNvsFormat(store);
}
