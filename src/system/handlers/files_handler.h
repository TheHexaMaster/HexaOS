/*
  HexaOS - files_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Unified filesystem domain handler for HexaOS.
  Owns the Files domain for all enabled storage backends — LittleFS flash
  and SD card. Exposes a uniform active-backend dispatch API so the command
  layer and other consumers never deal with backend selection or adapter
  details directly.

  Flash backend (LittleFS): gated by HX_ENABLE_FEATURE_LITTLEFS.
  SD card backend (SDMMC):  gated by HX_ENABLE_FEATURE_SD.
  The entire handler is gated by HX_ENABLE_MODULE_STORAGE.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_STORAGE

#include <stddef.h>
#include <stdint.h>
#include <WString.h>

// ---------------------------------------------------------------------------
// Common types
// ---------------------------------------------------------------------------

struct HxFileInfo {
  char   path[256];
  size_t size_bytes;
  bool   exists;
  bool   is_dir;
};

struct HxFilesInfo {
  bool        ready;
  bool        mounted;
  const char* partition_label;
  size_t      total_bytes;
  size_t      used_bytes;
  size_t      free_bytes;
};

typedef bool (*HxFilesListCallback)(const HxFileInfo* entry, void* user);

// ---------------------------------------------------------------------------
// Backend selection
// ---------------------------------------------------------------------------

typedef enum : uint8_t {
  HX_FILES_BACKEND_FLASH = 0,
  HX_FILES_BACKEND_SD    = 1
} HxFilesBackend;

void           FilesSetActiveBackend(HxFilesBackend backend);
HxFilesBackend FilesGetActiveBackend();
bool           FilesActiveIsMounted();
const char*    FilesActiveBackendName();

// ---------------------------------------------------------------------------
// Flash backend — gated by HX_ENABLE_FEATURE_LITTLEFS
// ---------------------------------------------------------------------------

#if HX_ENABLE_FEATURE_LITTLEFS

bool FilesInit();
bool FilesMount();
bool FilesUnmount();
bool FilesFormat();

bool   FilesExists(const char* path);
bool   FilesRemove(const char* path);
bool   FilesRename(const char* old_path, const char* new_path);
bool   FilesMkdir(const char* path);
bool   FilesRmdir(const char* path);

bool   FilesStat(const char* path, HxFileInfo* out_info);
size_t FilesSize(const char* path);
bool   FilesIsFile(const char* path);
bool   FilesIsDir(const char* path);
bool   FilesGetInfo(HxFilesInfo* out_info);
bool   FilesList(const char* path, HxFilesListCallback callback, void* user);

String FilesReadText(const char* path);
bool   FilesReadBytes(const char* path, uint8_t* out_data, size_t out_size, size_t* out_len);

bool   FilesWriteText(const char* path, const char* text);
bool   FilesAppendText(const char* path, const char* text);
bool   FilesWriteBytes(const char* path, const uint8_t* data, size_t len);
bool   FilesAppendBytes(const char* path, const uint8_t* data, size_t len);

bool   FilesWriteTextAtomic(const char* path, const char* text);
bool   FilesWriteBytesAtomic(const char* path, const uint8_t* data, size_t len);

#endif // HX_ENABLE_FEATURE_LITTLEFS

// ---------------------------------------------------------------------------
// SD card backend lifecycle — gated by HX_ENABLE_FEATURE_SD
// ---------------------------------------------------------------------------

#if HX_ENABLE_FEATURE_SD

bool FilesSdInit();
bool FilesSdMount();
bool FilesSdUnmount();
bool FilesSdIsMounted();
bool FilesSdCheckHealth();
bool FilesSdGetStorageInfo(uint64_t* out_total, uint64_t* out_used);

#endif // HX_ENABLE_FEATURE_SD

// ---------------------------------------------------------------------------
// Active-backend dispatch operations
// All functions below operate on whichever backend is currently selected
// via FilesSetActiveBackend(). Callers do not need to know which backend
// is active.
// ---------------------------------------------------------------------------

bool FilesActiveExists(const char* path);
bool FilesActiveRemove(const char* path);
bool FilesActiveRename(const char* old_path, const char* new_path);
bool FilesActiveMkdir(const char* path);
bool FilesActiveRmdir(const char* path);
bool FilesActiveStat(const char* path, HxFileInfo* out_info);
bool FilesActiveList(const char* path, HxFilesListCallback callback, void* user);
bool FilesActiveReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len);
bool FilesActiveWriteText(const char* path, const char* text);
bool FilesActiveAppendText(const char* path, const char* text);
bool FilesActiveWriteBytes(const char* path, const uint8_t* data, size_t len);
bool FilesActiveAppendBytes(const char* path, const uint8_t* data, size_t len);
bool FilesActiveFormat();

#endif // HX_ENABLE_MODULE_STORAGE
