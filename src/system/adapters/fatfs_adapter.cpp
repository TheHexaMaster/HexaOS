/*
  HexaOS - fatfs_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  FatFS adapter stub implementation.
  All file operations return false pending integration of the ESP-IDF FatFS stack
  on top of the SDMMC hardware adapter.
*/

#include "fatfs_adapter.h"

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_SD

#include <stddef.h>

#include "sdmmc_adapter.h"
#include "system/core/log.h"

bool FatInit() {
  return SdmmcInit();
}

bool FatMount() {
  LogWarn("FAT: mount not implemented (stub)");
  return false;
}

bool FatUnmount() {
  SdmmcDeinit();
  return true;
}

bool FatFormat() {
  LogWarn("FAT: format not implemented (stub)");
  return false;
}

bool FatExists(const char* path)                                                     { (void)path; return false; }
bool FatRemove(const char* path)                                                     { (void)path; return false; }
bool FatRename(const char* old_path, const char* new_path)                           { (void)old_path; (void)new_path; return false; }
bool FatMkdir(const char* path)                                                      { (void)path; return false; }
bool FatRmdir(const char* path)                                                      { (void)path; return false; }
bool FatStat(const char* path, bool* out_is_dir, size_t* out_size)                  { (void)path; (void)out_is_dir; (void)out_size; return false; }
bool FatGetStorageInfo(size_t* out_total, size_t* out_used)                          { (void)out_total; (void)out_used; return false; }
bool FatList(const char* path, FatListCallback callback, void* user)                 { (void)path; (void)callback; (void)user; return false; }
bool FatReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len) { (void)path; (void)out; (void)out_size; (void)out_len; return false; }
bool FatWriteBytes(const char* path, const uint8_t* data, size_t len, bool append)  { (void)path; (void)data; (void)len; (void)append; return false; }

#endif // HX_ENABLE_FEATURE_SD
