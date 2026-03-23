/*
  HexaOS - mod_storage.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Storage lifecycle module for HexaOS.
  Coordinates independent initialisation and mount of each enabled storage
  backend. Each backend fails independently — an error on one does not
  prevent the other from starting.

  Backends managed here:
    - LittleFS (internal flash): via files_handler.
      Gated by HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS.
    - SDMMC + FatFS (SD card): via sdmmc_adapter.
      Gated by HX_ENABLE_FEATURE_SD.
      Mount failure is expected when no card is inserted.
*/

#include "system/core/log.h"
#include "system/core/module_registry.h"
#include "system/core/scheduler.h"

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS
  #include "system/handlers/files_handler.h"
#endif

#if HX_ENABLE_FEATURE_SD
  #include "system/adapters/sdmmc_adapter.h"
#endif

static constexpr const char* HX_STO_TAG = "STO";

#if HX_ENABLE_FEATURE_SD
static HxScheduler g_sd_check_sched;
#endif

static bool StorageInit() {
  bool ok = true;

#if HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS
  if (!FilesInit()) {
    HX_LOGE(HX_STO_TAG, "LittleFS init failed");
    ok = false;
  }
#endif

#if HX_ENABLE_FEATURE_SD
  if (!SdmmcInit()) {
    // Not a hard failure — missing or unconnected SD card is normal.
    HX_LOGE(HX_STO_TAG, "SDMMC init failed (no card or pinmap incomplete)");
  }
#endif

  return ok;
}

static void StorageStart() {
#if HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS
  if (!FilesMount()) {
    HX_LOGE(HX_STO_TAG, "LittleFS mount failed");
  }
#endif

#if HX_ENABLE_FEATURE_SD
  HxSchedulerInit(&g_sd_check_sched, HX_STORAGE_SD_CHECK_INTERVAL_MS, 0);
  if (!SdmmcMount()) {
    HX_LOGW(HX_STO_TAG, "SDMMC mount failed (card may not be inserted)");
  }
#endif
}

static void StorageLoop() {
}

static void StorageEvery10ms() {
}

static void StorageEvery100ms() {
}

static void StorageEverySecond() {
#if HX_ENABLE_FEATURE_SD
  if (!HxSchedulerDue(&g_sd_check_sched)) { return; }

  if (SdmmcIsMounted()) {
    HX_LOGLL(HX_STO_TAG, "SD check: mounted");
    // Health check: send CMD13 to the physical card; unmount if it no longer responds.
    if (!SdmmcCheckHealth()) {
      HX_LOGW(HX_STO_TAG, "SD card removed — unmounting");
      SdmmcUnmount();
    }
  } else {
    HX_LOGLL(HX_STO_TAG, "SD check: not mounted — attempting mount");
    // Card not mounted — try to bring it up.
    if (SdmmcMount()) {
      HX_LOGI(HX_STO_TAG, "SD card mounted");
    }
  }
#endif
}

const HxModule ModuleStorage = {
  .name        = "storage",
  .init        = StorageInit,
  .start       = StorageStart,
  .loop        = StorageLoop,
  .every_10ms  = StorageEvery10ms,
  .every_100ms = StorageEvery100ms,
  .every_1s    = StorageEverySecond
};
