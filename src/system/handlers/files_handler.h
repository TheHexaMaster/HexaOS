/*
  HexaOS - files_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  LittleFS filesystem handler for HexaOS internal flash storage.
  Provides thread-safe file and directory operations backed by the LittleFS
  adapter. Gated by HX_ENABLE_MODULE_STORAGE and HX_ENABLE_FEATURE_LITTLEFS.

  SD card storage is a separate backend managed independently by mod_storage
  via sdmmc_adapter and is available through POSIX VFS at /sd.
*/

#pragma once

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS

#include <stddef.h>
#include <stdint.h>
#include <WString.h>

struct HxFileInfo {
  char path[256];
  size_t size_bytes;
  bool exists;
  bool is_dir;
};

struct HxFilesInfo {
  bool ready;
  bool mounted;
  const char* partition_label;
  size_t total_bytes;
  size_t used_bytes;
  size_t free_bytes;
};

typedef bool (*HxFilesListCallback)(const HxFileInfo* entry, void* user);

bool FilesInit();
bool FilesMount();
bool FilesUnmount();
bool FilesFormat();

bool FilesExists(const char* path);
bool FilesRemove(const char* path);
bool FilesRename(const char* old_path, const char* new_path);
bool FilesMkdir(const char* path);
bool FilesRmdir(const char* path);

bool FilesStat(const char* path, HxFileInfo* out_info);
size_t FilesSize(const char* path);
bool FilesIsFile(const char* path);
bool FilesIsDir(const char* path);
bool FilesGetInfo(HxFilesInfo* out_info);
bool FilesList(const char* path, HxFilesListCallback callback, void* user);

String FilesReadText(const char* path);
bool FilesReadBytes(const char* path, uint8_t* out_data, size_t out_size, size_t* out_len);

bool FilesWriteText(const char* path, const char* text);
bool FilesAppendText(const char* path, const char* text);
bool FilesWriteBytes(const char* path, const uint8_t* data, size_t len);
bool FilesAppendBytes(const char* path, const uint8_t* data, size_t len);

bool FilesWriteTextAtomic(const char* path, const char* text);
bool FilesWriteBytesAtomic(const char* path, const uint8_t* data, size_t len);

#endif // HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS
