/*
  HexaOS - littlefs_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Raw LittleFS backend adapter for HexaOS internal flash storage.
  Exposes direct filesystem operations backed by the Arduino LittleFS library.
  This adapter is the only translation unit in HexaOS that includes <LittleFS.h>.
  No locking, no path validation — those are responsibilities of the calling handler.
  Gated by HX_ENABLE_FEATURE_LITTLEFS.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_LITTLEFS

#include <stddef.h>
#include <stdint.h>

// Callback for LfsList. Return false to stop enumeration early.
typedef bool (*LfsListCallback)(const char* name, bool is_dir, size_t size_bytes, void* user);

bool LfsInit(const char* partition_label);
bool LfsMount(const char* partition_label);
bool LfsUnmount();
bool LfsFormat(const char* partition_label);

bool LfsExists(const char* path);
bool LfsRemove(const char* path);
bool LfsRename(const char* old_path, const char* new_path);
bool LfsMkdir(const char* path);
bool LfsRmdir(const char* path);

// Returns basic metadata. Pass nullptr for out parameters you do not need.
bool LfsStat(const char* path, bool* out_is_dir, size_t* out_size);
bool LfsGetStorageInfo(size_t* out_total, size_t* out_used);
bool LfsList(const char* path, LfsListCallback callback, void* user);

bool LfsReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len);

// If append is true, appends to the file; otherwise overwrites.
bool LfsWriteBytes(const char* path, const uint8_t* data, size_t len, bool append);

#endif // HX_ENABLE_FEATURE_LITTLEFS
