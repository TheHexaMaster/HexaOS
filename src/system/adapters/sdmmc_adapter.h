/*
  HexaOS - sdmmc_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Unified SDMMC + FatFS adapter for HexaOS SD card storage.
  Handles SDMMC bus configuration (pin detection, 1-bit / 4-bit mode
  selection) and mounts the SD card via esp_vfs_fat so that all file
  operations are available through a standard POSIX interface.
  Exposes the same operation surface as littlefs_adapter so that
  files_handler can select backends uniformly via build flags.
  Gated by HX_ENABLE_FEATURE_SD.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_SD

#include <stddef.h>
#include <stdint.h>

// Callback for SdmmcList. Return false to stop enumeration early.
typedef bool (*SdmmcListCallback)(const char* name, bool is_dir, size_t size_bytes, void* user);

// Validate mandatory pin assignments and determine bus width from the pinmap.
// Must be called before SdmmcMount. Does not touch hardware.
bool SdmmcInit();

// Mount the SD card via SDMMC host + FatFS VFS. Configures the SDMMC
// peripheral using pins resolved in SdmmcInit.
bool SdmmcMount();

// Unmount the SD card and release the SDMMC host.
bool SdmmcUnmount();

// Not supported on SD cards — always returns false.
bool SdmmcFormat();

bool SdmmcExists(const char* path);
bool SdmmcRemove(const char* path);
bool SdmmcRename(const char* old_path, const char* new_path);
bool SdmmcMkdir(const char* path);
bool SdmmcRmdir(const char* path);

// Returns basic metadata. Pass nullptr for out parameters you do not need.
bool SdmmcStat(const char* path, bool* out_is_dir, size_t* out_size);
bool SdmmcGetStorageInfo(uint64_t* out_total, uint64_t* out_used);
bool SdmmcList(const char* path, SdmmcListCallback callback, void* user);

bool SdmmcReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len);

// If append is true, appends to the file; otherwise overwrites.
bool SdmmcWriteBytes(const char* path, const uint8_t* data, size_t len, bool append);

#endif // HX_ENABLE_FEATURE_SD
