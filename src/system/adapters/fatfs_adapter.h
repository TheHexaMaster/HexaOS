/*
  HexaOS - fatfs_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  FatFS filesystem adapter for HexaOS SD card storage.
  Provides the same file-operation surface as littlefs_adapter so that
  files_handler can select backends uniformly via build flags.
  Mount calls SdmmcInit internally. Gated by HX_ENABLE_FEATURE_SD.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_SD

#include <stddef.h>
#include <stdint.h>

// Callback for FatList. Return false to stop enumeration early.
typedef bool (*FatListCallback)(const char* name, bool is_dir, size_t size_bytes, void* user);

bool FatInit();
bool FatMount();
bool FatUnmount();
bool FatFormat();

bool FatExists(const char* path);
bool FatRemove(const char* path);
bool FatRename(const char* old_path, const char* new_path);
bool FatMkdir(const char* path);
bool FatRmdir(const char* path);

bool FatStat(const char* path, bool* out_is_dir, size_t* out_size);
bool FatGetStorageInfo(size_t* out_total, size_t* out_used);
bool FatList(const char* path, FatListCallback callback, void* user);

bool FatReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len);

// If append is true, appends to the file; otherwise overwrites.
bool FatWriteBytes(const char* path, const uint8_t* data, size_t len, bool append);

#endif // HX_ENABLE_FEATURE_SD
