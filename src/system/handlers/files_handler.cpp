/*
  HexaOS - files_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Unified filesystem domain handler for HexaOS.
  Owns the Files domain for all enabled storage backends. Manages lifecycle,
  mutex protection, path validation and atomic-write orchestration for the
  LittleFS flash backend. Provides SD card lifecycle delegation and a uniform
  active-backend dispatch layer so callers never deal with adapter selection.

  Flash backend gated by HX_ENABLE_FEATURE_LITTLEFS.
  SD backend gated by HX_ENABLE_FEATURE_SD.
  The entire handler is gated by HX_ENABLE_MODULE_STORAGE.
*/

#include "files_handler.h"

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "system/adapters/littlefs_adapter.h"
#include "system/core/log.h"
#include "system/core/rtos.h"
#include "system/core/runtime.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr const char* HX_FILES_TAG            = "FIL";
static constexpr const char* HX_FILES_PARTITION_LABEL = "littlefs";

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static bool           g_files_ready      = false;
static HxRtosCritical g_files_state_lock = HX_RTOS_CRITICAL_INIT;
static HxRtosMutex    g_files_mutex      = HX_RTOS_MUTEX_INIT;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void FilesSetMounted(bool mounted) {
  RtosCriticalEnter(&g_files_state_lock);
  Hx.files_mounted = mounted;
  RtosCriticalExit(&g_files_state_lock);
}

static bool FilesMountedState() {
  bool mounted = false;
  RtosCriticalEnter(&g_files_state_lock);
  mounted = Hx.files_mounted;
  RtosCriticalExit(&g_files_state_lock);
  return mounted;
}

static bool FilesTakeLock() {
  return RtosMutexLock(&g_files_mutex, HX_RTOS_WAIT_FOREVER);
}

static void FilesGiveLock() {
  RtosMutexUnlock(&g_files_mutex);
}

static bool FilesPathIsValid(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  if (path[0] != '/') {
    return false;
  }
  return (strlen(path) <= HX_FILES_PATH_MAX);
}

static void FilesFillEmptyInfo(HxFileInfo* out_info, const char* path) {
  if (!out_info) {
    return;
  }
  memset(out_info, 0, sizeof(*out_info));
  if (path && path[0]) {
    snprintf(out_info->path, sizeof(out_info->path), "%s", path);
  }
}

static bool FilesBuildTempPath(const char* path, char* out, size_t out_size) {
  if (!FilesPathIsValid(path) || !out || (out_size == 0)) {
    return false;
  }
  int n = snprintf(out, out_size, "%s.tmp", path);
  return (n > 0 && (size_t)n < out_size);
}

// ---------------------------------------------------------------------------
// Internal write helpers — called with lock already held
// ---------------------------------------------------------------------------

static bool FilesWriteBytesInternal(const char* path, const uint8_t* data, size_t len, bool append) {
  if (!FilesMountedState() || !FilesPathIsValid(path)) {
    return false;
  }
  if (len > 0 && !data) {
    return false;
  }
  if (!LfsWriteBytes(path, data, len, append)) {
    HX_LOGW(HX_FILES_TAG, "write failed path=%s append=%d", path, (int)append);
    return false;
  }
  return true;
}

static bool FilesWriteBytesAtomicInternal(const char* path, const uint8_t* data, size_t len) {
  if (!FilesMountedState() || !FilesPathIsValid(path)) {
    return false;
  }
  if (len > 0 && !data) {
    return false;
  }

  char temp_path[HX_FILES_PATH_MAX + 8];
  if (!FilesBuildTempPath(path, temp_path, sizeof(temp_path))) {
    return false;
  }

  LfsRemove(temp_path);

  if (!LfsWriteBytes(temp_path, data, len, false)) {
    LfsRemove(temp_path);
    return false;
  }

  if (LfsExists(path) && !LfsRemove(path)) {
    LfsRemove(temp_path);
    HX_LOGW(HX_FILES_TAG, "remove before rename failed path=%s", path);
    return false;
  }

  if (!LfsRename(temp_path, path)) {
    LfsRemove(temp_path);
    HX_LOGW(HX_FILES_TAG, "atomic rename failed path=%s", path);
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Bridge: LfsListCallback → HxFilesListCallback
// ---------------------------------------------------------------------------

struct FilesListBridge {
  HxFilesListCallback fn;
  void* user;
};

static bool FilesListBridgeCb(const char* name, bool is_dir, size_t size_bytes, void* user) {
  auto* bridge = static_cast<FilesListBridge*>(user);
  HxFileInfo info = {};
  snprintf(info.path, sizeof(info.path), "%s", name ? name : "");
  info.exists     = true;
  info.is_dir     = is_dir;
  info.size_bytes = size_bytes;
  return bridge->fn(&info, bridge->user);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool FilesInit() {
  if (g_files_ready) {
    return true;
  }

  if (!RtosCriticalInit(&g_files_state_lock)) {
    HX_LOGE(HX_FILES_TAG, "init failed: state lock");
    return false;
  }

  if (!RtosMutexInit(&g_files_mutex)) {
    HX_LOGE(HX_FILES_TAG, "init failed: mutex");
    RtosCriticalDestroy(&g_files_state_lock);
    return false;
  }

  FilesSetMounted(false);
  LfsInit(HX_FILES_PARTITION_LABEL);

  g_files_ready = true;
  HX_LOGI(HX_FILES_TAG, "init OK partition=%s", HX_FILES_PARTITION_LABEL);
  return true;
}

bool FilesMount() {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool mounted = LfsMount(HX_FILES_PARTITION_LABEL);
  FilesSetMounted(mounted);

  if (mounted) {
    size_t total = 0;
    size_t used  = 0;
    LfsGetStorageInfo(&total, &used);
    HX_LOGI(HX_FILES_TAG, "mount OK partition=%s total=%u used=%u",
            HX_FILES_PARTITION_LABEL, (unsigned)total, (unsigned)used);
  } else {
    HX_LOGE(HX_FILES_TAG, "mount failed partition=%s", HX_FILES_PARTITION_LABEL);
  }

  FilesGiveLock();
  return mounted;
}

bool FilesUnmount() {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  if (FilesMountedState()) {
    LfsUnmount();
    FilesSetMounted(false);
    HX_LOGI(HX_FILES_TAG, "unmount OK partition=%s", HX_FILES_PARTITION_LABEL);
  }

  FilesGiveLock();
  return true;
}

bool FilesFormat() {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool ok = LfsFormat(HX_FILES_PARTITION_LABEL);
  FilesSetMounted(ok);

  if (ok) {
    HX_LOGW(HX_FILES_TAG, "format OK partition=%s", HX_FILES_PARTITION_LABEL);
  } else {
    HX_LOGE(HX_FILES_TAG, "format failed partition=%s", HX_FILES_PARTITION_LABEL);
  }

  FilesGiveLock();
  return ok;
}

bool FilesExists(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMountedState() && FilesPathIsValid(path) && LfsExists(path);
  FilesGiveLock();
  return ok;
}

bool FilesRemove(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMountedState() && FilesPathIsValid(path) && LfsRemove(path);
  FilesGiveLock();
  return ok;
}

bool FilesRename(const char* old_path, const char* new_path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMountedState()
         && FilesPathIsValid(old_path)
         && FilesPathIsValid(new_path)
         && LfsRename(old_path, new_path);
  FilesGiveLock();
  return ok;
}

bool FilesMkdir(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMountedState() && FilesPathIsValid(path) && LfsMkdir(path);
  FilesGiveLock();
  return ok;
}

bool FilesRmdir(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMountedState() && FilesPathIsValid(path) && LfsRmdir(path);
  FilesGiveLock();
  return ok;
}

bool FilesStat(const char* path, HxFileInfo* out_info) {
  FilesFillEmptyInfo(out_info, path);

  if (!out_info || !g_files_ready || !FilesTakeLock()) {
    return false;
  }

  if (!FilesMountedState() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return false;
  }

  bool   is_dir = false;
  size_t size   = 0;
  bool   ok     = LfsStat(path, &is_dir, &size);

  if (ok) {
    out_info->exists    = true;
    out_info->is_dir    = is_dir;
    out_info->size_bytes = size;
    snprintf(out_info->path, sizeof(out_info->path), "%s", path);
  }

  FilesGiveLock();
  return ok;
}

size_t FilesSize(const char* path) {
  HxFileInfo info = {};
  if (!FilesStat(path, &info) || info.is_dir) {
    return 0;
  }
  return info.size_bytes;
}

bool FilesIsFile(const char* path) {
  HxFileInfo info = {};
  return FilesStat(path, &info) && info.exists && !info.is_dir;
}

bool FilesIsDir(const char* path) {
  HxFileInfo info = {};
  return FilesStat(path, &info) && info.exists && info.is_dir;
}

bool FilesGetInfo(HxFilesInfo* out_info) {
  if (!out_info || !g_files_ready) {
    return false;
  }

  memset(out_info, 0, sizeof(*out_info));
  out_info->ready           = g_files_ready;
  out_info->mounted         = FilesMountedState();
  out_info->partition_label = HX_FILES_PARTITION_LABEL;

  if (!FilesTakeLock()) {
    return false;
  }

  if (FilesMountedState()) {
    size_t total = 0;
    size_t used  = 0;
    LfsGetStorageInfo(&total, &used);
    out_info->total_bytes = total;
    out_info->used_bytes  = used;
    out_info->free_bytes  = (total >= used) ? (total - used) : 0;
  }

  FilesGiveLock();
  return true;
}

bool FilesList(const char* path, HxFilesListCallback callback, void* user) {
  if (!callback || !g_files_ready || !FilesTakeLock()) {
    return false;
  }

  if (!FilesMountedState() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return false;
  }

  FilesListBridge bridge = { callback, user };
  bool ok = LfsList(path, FilesListBridgeCb, &bridge);
  FilesGiveLock();
  return ok;
}

String FilesReadText(const char* path) {
  String out;

  if (!g_files_ready || !FilesTakeLock()) {
    return out;
  }

  if (!FilesMountedState() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return out;
  }

  bool   is_dir = false;
  size_t fsize  = 0;
  if (!LfsStat(path, &is_dir, &fsize) || is_dir || fsize == 0) {
    FilesGiveLock();
    return out;
  }

  uint8_t* buf = static_cast<uint8_t*>(malloc(fsize + 1));
  if (!buf) {
    FilesGiveLock();
    return out;
  }

  size_t read_len = 0;
  if (LfsReadBytes(path, buf, fsize, &read_len) && read_len > 0) {
    buf[read_len] = '\0';
    out = String(reinterpret_cast<const char*>(buf));
  }

  free(buf);
  FilesGiveLock();
  return out;
}

bool FilesReadBytes(const char* path, uint8_t* out_data, size_t out_size, size_t* out_len) {
  if (out_len) {
    *out_len = 0;
  }

  if (!out_data || (out_size == 0) || !g_files_ready || !FilesTakeLock()) {
    return false;
  }

  if (!FilesMountedState() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return false;
  }

  bool ok = LfsReadBytes(path, out_data, out_size, out_len);
  FilesGiveLock();
  return ok;
}

bool FilesWriteText(const char* path, const char* text) {
  const char* safe = text ? text : "";
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesInternal(path,
                                    reinterpret_cast<const uint8_t*>(safe),
                                    strlen(safe), false);
  FilesGiveLock();
  return ok;
}

bool FilesAppendText(const char* path, const char* text) {
  const char* safe = text ? text : "";
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesInternal(path,
                                    reinterpret_cast<const uint8_t*>(safe),
                                    strlen(safe), true);
  FilesGiveLock();
  return ok;
}

bool FilesWriteBytes(const char* path, const uint8_t* data, size_t len) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesInternal(path, data, len, false);
  FilesGiveLock();
  return ok;
}

bool FilesAppendBytes(const char* path, const uint8_t* data, size_t len) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesInternal(path, data, len, true);
  FilesGiveLock();
  return ok;
}

bool FilesWriteTextAtomic(const char* path, const char* text) {
  const char* safe = text ? text : "";
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesAtomicInternal(path,
                                          reinterpret_cast<const uint8_t*>(safe),
                                          strlen(safe));
  FilesGiveLock();
  return ok;
}

bool FilesWriteBytesAtomic(const char* path, const uint8_t* data, size_t len) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesAtomicInternal(path, data, len);
  FilesGiveLock();
  return ok;
}

#endif // HX_ENABLE_MODULE_STORAGE && HX_ENABLE_FEATURE_LITTLEFS

// ---------------------------------------------------------------------------
// SD card backend lifecycle + backend selection + active-backend dispatch
// All gated by HX_ENABLE_MODULE_STORAGE.
// ---------------------------------------------------------------------------

#if HX_ENABLE_MODULE_STORAGE

#include <string.h>

#if HX_ENABLE_FEATURE_SD
  #include "system/adapters/sdmmc_adapter.h"
#endif

#include "system/core/runtime.h"

// ---------------------------------------------------------------------------
// SD card backend lifecycle
// ---------------------------------------------------------------------------

#if HX_ENABLE_FEATURE_SD

bool FilesSdInit() {
  return SdmmcInit();
}

bool FilesSdMount() {
  return SdmmcMount();
}

bool FilesSdUnmount() {
  return SdmmcUnmount();
}

bool FilesSdIsMounted() {
  return SdmmcIsMounted();
}

bool FilesSdCheckHealth() {
  return SdmmcCheckHealth();
}

bool FilesSdGetStorageInfo(uint64_t* out_total, uint64_t* out_used) {
  return SdmmcGetStorageInfo(out_total, out_used);
}

#endif // HX_ENABLE_FEATURE_SD

// ---------------------------------------------------------------------------
// Backend selection
// ---------------------------------------------------------------------------

static HxFilesBackend g_active_backend = HX_FILES_BACKEND_FLASH;

void FilesSetActiveBackend(HxFilesBackend backend) {
  g_active_backend = backend;
}

HxFilesBackend FilesGetActiveBackend() {
  return g_active_backend;
}

bool FilesActiveIsMounted() {
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD) {
    return SdmmcIsMounted();
  }
#endif
  return Hx.files_mounted;
}

const char* FilesActiveBackendName() {
  return (g_active_backend == HX_FILES_BACKEND_SD) ? "sd" : "flash";
}

// ---------------------------------------------------------------------------
// Active-backend file operations
// ---------------------------------------------------------------------------

bool FilesActiveExists(const char* path) {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesExists(path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcExists(path); }
#endif
  return false;
}

bool FilesActiveRemove(const char* path) {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesRemove(path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcRemove(path); }
#endif
  return false;
}

bool FilesActiveRename(const char* old_path, const char* new_path) {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesRename(old_path, new_path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcRename(old_path, new_path); }
#endif
  return false;
}

bool FilesActiveMkdir(const char* path) {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesMkdir(path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcMkdir(path); }
#endif
  return false;
}

bool FilesActiveRmdir(const char* path) {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesRmdir(path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcRmdir(path); }
#endif
  return false;
}

bool FilesActiveStat(const char* path, HxFileInfo* out_info) {
  if (out_info) {
    memset(out_info, 0, sizeof(*out_info));
    if (path && path[0]) {
      snprintf(out_info->path, sizeof(out_info->path), "%s", path);
    }
  }
  if (!out_info || !FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesStat(path, out_info); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD) {
    bool   is_dir = false;
    size_t size   = 0;
    if (!SdmmcStat(path, &is_dir, &size)) { return false; }
    out_info->exists     = true;
    out_info->is_dir     = is_dir;
    out_info->size_bytes = size;
    snprintf(out_info->path, sizeof(out_info->path), "%s", path);
    return true;
  }
#endif
  return false;
}

bool FilesActiveList(const char* path, HxFilesListCallback callback, void* user) {
  if (!FilesActiveIsMounted() || !callback) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesList(path, callback, user); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD) {
    struct SdListBridge {
      HxFilesListCallback fn;
      void*               user;
    };
    SdListBridge bridge = { callback, user };
    auto sd_cb = [](const char* name, bool is_dir, size_t size_bytes, void* u) -> bool {
      auto* b = static_cast<SdListBridge*>(u);
      HxFileInfo info = {};
      snprintf(info.path, sizeof(info.path), "%s", name ? name : "");
      info.exists     = true;
      info.is_dir     = is_dir;
      info.size_bytes = size_bytes;
      return b->fn(&info, b->user);
    };
    return SdmmcList(path, sd_cb, &bridge);
  }
#endif
  return false;
}

bool FilesActiveReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len) {
  if (out_len) { *out_len = 0; }
  if (!out || (out_size == 0) || !FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesReadBytes(path, out, out_size, out_len); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcReadBytesCapped(path, out, out_size, out_len); }
#endif
  return false;
}

bool FilesActiveWriteText(const char* path, const char* text) {
  if (!FilesActiveIsMounted()) { return false; }
  const char* safe = text ? text : "";
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesWriteText(path, safe); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD) {
    return SdmmcWriteBytes(path, reinterpret_cast<const uint8_t*>(safe), strlen(safe), false);
  }
#endif
  return false;
}

bool FilesActiveAppendText(const char* path, const char* text) {
  if (!FilesActiveIsMounted()) { return false; }
  const char* safe = text ? text : "";
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesAppendText(path, safe); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD) {
    return SdmmcWriteBytes(path, reinterpret_cast<const uint8_t*>(safe), strlen(safe), true);
  }
#endif
  return false;
}

bool FilesActiveWriteBytes(const char* path, const uint8_t* data, size_t len) {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesWriteBytes(path, data, len); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcWriteBytes(path, data, len, false); }
#endif
  return false;
}

bool FilesActiveAppendBytes(const char* path, const uint8_t* data, size_t len) {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesAppendBytes(path, data, len); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcWriteBytes(path, data, len, true); }
#endif
  return false;
}

bool FilesActiveFormat() {
  if (!FilesActiveIsMounted()) { return false; }
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active_backend == HX_FILES_BACKEND_FLASH) { return FilesFormat(); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active_backend == HX_FILES_BACKEND_SD)    { return SdmmcFormat(); }
#endif
  return false;
}

#endif // HX_ENABLE_MODULE_STORAGE
