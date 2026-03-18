/*
  HexaOS - esp_nvs.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  ESP NVS platform adapter.
  Implements the concrete non-volatile storage backend used by HexaOS services to open namespaces and read, write or commit primitive persisted values.
*/

#include "hexaos.h"
#include "headers/hx_platform_nvs.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <stdlib.h>

static nvs_handle_t g_nvs_config = 0;
static nvs_handle_t g_nvs_state = 0;
static nvs_handle_t g_nvs_factory = 0;

static nvs_handle_t GetStoreHandle(HxNvsStore store) {
  switch (store) {
    case HX_NVS_STORE_CONFIG:  return g_nvs_config;
    case HX_NVS_STORE_STATE:   return g_nvs_state;
    case HX_NVS_STORE_FACTORY: return g_nvs_factory;
    default:                   return 0;
  }
}

static bool IsHandleReady(nvs_handle_t handle) {
  return handle != 0;
}

bool EspNvsInit() {
  esp_err_t err = nvs_flash_init();

  if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      LogError("NVS erase failed: %d", (int)err);
      return false;
    }

    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    LogError("NVS init failed: %d", (int)err);
    return false;
  }

  LogInfo("NVS init OK");
  return true;
}

bool EspNvsOpenConfig() {
  if (IsHandleReady(g_nvs_config)) {
    return true;
  }

  esp_err_t err = nvs_open("config", NVS_READWRITE, &g_nvs_config);
  if (err != ESP_OK) {
    LogError("NVS config open failed: %d", (int)err);
    return false;
  }

  return true;
}

bool EspNvsOpenState() {
  if (IsHandleReady(g_nvs_state)) {
    return true;
  }

  esp_err_t err = nvs_open("state", NVS_READWRITE, &g_nvs_state);
  if (err != ESP_OK) {
    LogError("NVS state open failed: %d", (int)err);
    return false;
  }

  return true;
}

bool EspNvsOpenFactory() {
  if (IsHandleReady(g_nvs_factory)) {
    return true;
  }

  esp_err_t err = nvs_open("factory", NVS_READWRITE, &g_nvs_factory);
  if (err != ESP_OK) {
    LogError("NVS factory open failed: %d", (int)err);
    return false;
  }

  return true;
}

bool HxNvsGetBool(HxNvsStore store, const char* key, bool* value) {
  if (!key || !value) {
    return false;
  }

  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  uint8_t raw = 0;
  esp_err_t err = nvs_get_u8(handle, key, &raw);
  if (err != ESP_OK) {
    return false;
  }

  *value = (raw != 0);
  return true;
}

bool HxNvsGetInt(HxNvsStore store, const char* key, int32_t* value) {
  if (!key || !value) {
    return false;
  }

  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  esp_err_t err = nvs_get_i32(handle, key, value);
  return (err == ESP_OK);
}

bool HxNvsGetString(HxNvsStore store, const char* key, String& value) {
  if (!key) {
    return false;
  }

  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  size_t size = 0;
  esp_err_t err = nvs_get_str(handle, key, nullptr, &size);
  if ((err != ESP_OK) || (size == 0)) {
    return false;
  }

  char* buffer = static_cast<char*>(malloc(size));
  if (!buffer) {
    return false;
  }

  err = nvs_get_str(handle, key, buffer, &size);
  if (err == ESP_OK) {
    value = String(buffer);
    free(buffer);
    return true;
  }

  free(buffer);
  return false;
}

bool HxNvsSetBool(HxNvsStore store, const char* key, bool value) {
  if (!key) {
    return false;
  }

  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  esp_err_t err = nvs_set_u8(handle, key, value ? 1 : 0);
  return (err == ESP_OK);
}

bool HxNvsSetInt(HxNvsStore store, const char* key, int32_t value) {
  if (!key) {
    return false;
  }

  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  esp_err_t err = nvs_set_i32(handle, key, value);
  return (err == ESP_OK);
}

bool HxNvsSetString(HxNvsStore store, const char* key, const char* value) {
  if (!key || !value) {
    return false;
  }

  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  esp_err_t err = nvs_set_str(handle, key, value);
  return (err == ESP_OK);
}

bool HxNvsEraseKey(HxNvsStore store, const char* key) {
  if (!key) {
    return false;
  }

  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  esp_err_t err = nvs_erase_key(handle, key);
  return (err == ESP_OK) || (err == ESP_ERR_NVS_NOT_FOUND);
}

bool HxNvsCommit(HxNvsStore store) {
  nvs_handle_t handle = GetStoreHandle(store);
  if (!IsHandleReady(handle)) {
    return false;
  }

  esp_err_t err = nvs_commit(handle);
  return (err == ESP_OK);
}