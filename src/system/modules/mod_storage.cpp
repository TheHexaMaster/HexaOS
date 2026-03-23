/*
  HexaOS - mod_storage.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Storage lifecycle module for HexaOS.
  Owns the init and start hooks for the filesystem subsystem. FilesInit is
  called during module init (after boot), FilesMount during module start.
  Gated by HX_ENABLE_MODULE_STORAGE.
*/

#include "system/core/log.h"
#include "system/core/module_registry.h"

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_STORAGE
  #include "system/handlers/files_handler.h"
#endif

static bool StorageInit() {
#if HX_ENABLE_MODULE_STORAGE
  if (!FilesInit()) {
    LogWarn("STO: files init failed");
    return false;
  }
#endif
  return true;
}

static void StorageStart() {
#if HX_ENABLE_MODULE_STORAGE
  if (!FilesMount()) {
    LogWarn("STO: files mount failed");
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
