/*
  HexaOS - esp_nvs.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  ESP NVS platform adapter.
  Implements the concrete non-volatile storage backend used by HexaOS services to open dedicated NVS partitions and read, write or commit primitive persisted values.
*/

#include "esp_nvs.h"
#include "hexaos.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <stdlib.h>

static constexpr const char* HX_NVS_PARTITION_CONFIG = "nvs";
static constexpr const char* HX_NVS_PARTITION_STATE = "nvs_state";
static constexpr const char* HX_NVS_PARTITION_FACTORY = "nvs_factory";
static constexpr const char* HX_NVS_NAMESPACE = "hx";

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

static bool InitPartition(const char* label) {
  if (!label || !label[0]) {
    return false;
  }

  esp_err_t err = nvs_flash_init_partition(label);

  if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    LogWarn("NVS partition '%s' requires erase: %d", label, (int)err);

    err = nvs_flash_erase_partition(label);
    if (err != ESP_OK) {
      LogError("NVS partition '%s' erase failed: %d", label, (int)err);
      return false;
    }

    err = nvs_flash_init_partition(label);
  }

  if (err != ESP_OK) {
    LogError("NVS partition '%s' init failed: %d", label, (int)err);
    return false;
  }

  return true;
}

static bool OpenPartitionHandle(const char* partition_label, nvs_handle_t* handle) {
  if (!partition_label || !partition_label[0] || !handle) {
    return false;
  }

  if (IsHandleReady(*handle)) {
    return true;
  }

  esp_err_t err = nvs_open_from_partition(partition_label,
                                          HX_NVS_NAMESPACE,
                                          NVS_READWRITE,
                                          handle);
  if (err != ESP_OK) {
    LogError("NVS open failed: partition='%s' namespace='%s' err=%d",
             partition_label,
             HX_NVS_NAMESPACE,
             (int)err);
    return false;
  }

  return true;
}

bool EspNvsInit() {
  if (!InitPartition(HX_NVS_PARTITION_CONFIG)) {
    return false;
  }

  if (!InitPartition(HX_NVS_PARTITION_STATE)) {
    return false;
  }

  if (!InitPartition(HX_NVS_PARTITION_FACTORY)) {
    return false;
  }

  LogInfo("NVS init OK (config=%s state=%s factory=%s)",
          HX_NVS_PARTITION_CONFIG,
          HX_NVS_PARTITION_STATE,
          HX_NVS_PARTITION_FACTORY);
  return true;
}

bool EspNvsOpenConfig() {
  return OpenPartitionHandle(HX_NVS_PARTITION_CONFIG, &g_nvs_config);
}

bool EspNvsOpenState() {
  return OpenPartitionHandle(HX_NVS_PARTITION_STATE, &g_nvs_state);
}

bool EspNvsOpenFactory() {
  return OpenPartitionHandle(HX_NVS_PARTITION_FACTORY, &g_nvs_factory);
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