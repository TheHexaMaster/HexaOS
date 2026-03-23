/*
  HexaOS - files_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Unified filesystem handler implementation.
  Owns lifecycle, mutex, path validation, and atomic-write orchestration.
  Delegates all raw filesystem I/O to the active backend adapter selected
  at compile time (LittleFS for internal flash, FatFS for SD card).
  Gated by HX_ENABLE_MODULE_STORAGE.
*/

#include "files_handler.h"

#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_STORAGE

#if !HX_ENABLE_FEATURE_LITTLEFS && !HX_ENABLE_FEATURE_SD
  #error "HX_ENABLE_MODULE_STORAGE requires at least one storage backend (HX_ENABLE_FEATURE_LITTLEFS or HX_ENABLE_FEATURE_SD)"
#endif

#include <stdio.h>
#include <string.h>

#include "system/core/log.h"
#include "system/core/rtos.h"
#include "system/core/runtime.h"

#if HX_ENABLE_FEATURE_LITTLEFS
  #include "system/adapters/littlefs_adapter.h"
#endif

#if HX_ENABLE_FEATURE_SD
  #include "system/adapters/fatfs_adapter.h"
#endif

// ---------------------------------------------------------------------------
// Backend selection — maps generic FilesBackend* calls to the active adapter.
// When HX_ENABLE_FEATURE_LITTLEFS is set, LittleFS on internal flash is used.
// Otherwise, FatFS on SD card is used. Adding a third backend follows the
// same pattern — extend the chain here only.
// ---------------------------------------------------------------------------

#if HX_ENABLE_FEATURE_LITTLEFS
  static constexpr const char* HX_FILES_PARTITION_LABEL = "littlefs";
  #define FilesBackendInit(label)                   LfsInit(label)
  #define FilesBackendMount(label)                  LfsMount(label)
  #define FilesBackendUnmount()                     LfsUnmount()
  #define FilesBackendFormat(label)                 LfsFormat(label)
  #define FilesBackendExists(p)                     LfsExists(p)
  #define FilesBackendRemove(p)                     LfsRemove(p)
  #define FilesBackendRename(op, np)                LfsRename(op, np)
  #define FilesBackendMkdir(p)                      LfsMkdir(p)
  #define FilesBackendRmdir(p)                      LfsRmdir(p)
  #define FilesBackendStat(p, d, s)                 LfsStat(p, d, s)
  #define FilesBackendGetStorageInfo(t, u)          LfsGetStorageInfo(t, u)
  #define FilesBackendList(p, cb, u)                LfsList(p, cb, u)
  #define FilesBackendReadBytes(p, o, os, ol)       LfsReadBytes(p, o, os, ol)
  #define FilesBackendWriteBytes(p, d, l, a)        LfsWriteBytes(p, d, l, a)
  typedef LfsListCallback FilesBackendListCallback;
#elif HX_ENABLE_FEATURE_SD
  static constexpr const char* HX_FILES_PARTITION_LABEL = "sd";
  #define FilesBackendInit(label)                   FatInit()
  #define FilesBackendMount(label)                  FatMount()
  #define FilesBackendUnmount()                     FatUnmount()
  #define FilesBackendFormat(label)                 FatFormat()
  #define FilesBackendExists(p)                     FatExists(p)
  #define FilesBackendRemove(p)                     FatRemove(p)
  #define FilesBackendRename(op, np)                FatRename(op, np)
  #define FilesBackendMkdir(p)                      FatMkdir(p)
  #define FilesBackendRmdir(p)                      FatRmdir(p)
  #define FilesBackendStat(p, d, s)                 FatStat(p, d, s)
  #define FilesBackendGetStorageInfo(t, u)          FatGetStorageInfo(t, u)
  #define FilesBackendList(p, cb, u)                FatList(p, cb, u)
  #define FilesBackendReadBytes(p, o, os, ol)       FatReadBytes(p, o, os, ol)
  #define FilesBackendWriteBytes(p, d, l, a)        FatWriteBytes(p, d, l, a)
  typedef FatListCallback FilesBackendListCallback;
#endif

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static constexpr const char* HX_FILES_TAG      = "FIL";
static constexpr size_t      HX_FILES_PATH_MAX = 255;

static bool          g_files_ready      = false;
static HxRtosCritical g_files_state_lock = HX_RTOS_CRITICAL_INIT;
static HxRtosMutex   g_files_mutex      = HX_RTOS_MUTEX_INIT;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void FilesSetMounted(bool mounted) {
  RtosCriticalEnter(&g_files_state_lock);
  Hx.files_mounted = mounted;
  RtosCriticalExit(&g_files_state_lock);
}

static bool FilesMounted() {
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

static bool FilesBuildTempPath(const char* path, char* out_temp_path, size_t out_size) {
  if (!FilesPathIsValid(path) || !out_temp_path || (out_size == 0)) {
    return false;
  }
  int written = snprintf(out_temp_path, out_size, "%s.tmp", path);
  if ((written <= 0) || ((size_t)written >= out_size)) {
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Internal write helpers — called with lock already held
// ---------------------------------------------------------------------------

static bool FilesWriteBytesInternal(const char* path, const uint8_t* data, size_t len, bool append) {
  if (!FilesMounted() || !FilesPathIsValid(path)) {
    return false;
  }
  if (len > 0 && !data) {
    return false;
  }
  if (!FilesBackendWriteBytes(path, data, len, append)) {
    HX_LOGW(HX_FILES_TAG, "write failed path=%s append=%d", path, (int)append);
    return false;
  }
  return true;
}

static bool FilesWriteBytesAtomicInternal(const char* path, const uint8_t* data, size_t len) {
  if (!FilesMounted() || !FilesPathIsValid(path)) {
    return false;
  }
  if (len > 0 && !data) {
    return false;
  }

  char temp_path[HX_FILES_PATH_MAX + 8];
  if (!FilesBuildTempPath(path, temp_path, sizeof(temp_path))) {
    return false;
  }

  FilesBackendRemove(temp_path);

  if (!FilesBackendWriteBytes(temp_path, data, len, false)) {
    FilesBackendRemove(temp_path);
    return false;
  }

  if (FilesBackendExists(path) && !FilesBackendRemove(path)) {
    FilesBackendRemove(temp_path);
    HX_LOGW(HX_FILES_TAG, "remove before rename failed path=%s", path);
    return false;
  }

  if (!FilesBackendRename(temp_path, path)) {
    FilesBackendRemove(temp_path);
    HX_LOGW(HX_FILES_TAG, "atomic rename failed path=%s", path);
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Bridge: adapter list callback → HxFilesListCallback
// ---------------------------------------------------------------------------

struct FilesListBridge {
  HxFilesListCallback fn;
  void* user;
};

static bool FilesListBridgeCb(const char* name, bool is_dir, size_t size_bytes, void* user) {
  auto* bridge = static_cast<FilesListBridge*>(user);
  HxFileInfo info = {};
  snprintf(info.path, sizeof(info.path), "%s", name ? name : "");
  info.exists    = true;
  info.is_dir    = is_dir;
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

  if (!FilesBackendInit(HX_FILES_PARTITION_LABEL)) {
    HX_LOGE(HX_FILES_TAG, "backend init failed partition=%s", HX_FILES_PARTITION_LABEL);
    RtosMutexDestroy(&g_files_mutex);
    RtosCriticalDestroy(&g_files_state_lock);
    return false;
  }

  g_files_ready = true;
  HX_LOGI(HX_FILES_TAG, "init OK partition=%s", HX_FILES_PARTITION_LABEL);
  return true;
}

bool FilesMount() {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool mounted = FilesBackendMount(HX_FILES_PARTITION_LABEL);
  FilesSetMounted(mounted);

  if (mounted) {
    size_t total = 0;
    size_t used  = 0;
    FilesBackendGetStorageInfo(&total, &used);
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

  bool was_mounted = FilesMounted();
  if (was_mounted) {
    FilesBackendUnmount();
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

  bool formatted = FilesBackendFormat(HX_FILES_PARTITION_LABEL);
  FilesSetMounted(formatted);

  if (formatted) {
    HX_LOGW(HX_FILES_TAG, "format OK partition=%s", HX_FILES_PARTITION_LABEL);
  } else {
    HX_LOGE(HX_FILES_TAG, "format failed partition=%s", HX_FILES_PARTITION_LABEL);
  }

  FilesGiveLock();
  return formatted;
}

bool FilesExists(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool exists = FilesMounted() && FilesPathIsValid(path) && FilesBackendExists(path);
  FilesGiveLock();
  return exists;
}

bool FilesRemove(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMounted() && FilesPathIsValid(path) && FilesBackendRemove(path);
  FilesGiveLock();
  return ok;
}

bool FilesRename(const char* old_path, const char* new_path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMounted()
         && FilesPathIsValid(old_path)
         && FilesPathIsValid(new_path)
         && FilesBackendRename(old_path, new_path);
  FilesGiveLock();
  return ok;
}

bool FilesMkdir(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMounted() && FilesPathIsValid(path) && FilesBackendMkdir(path);
  FilesGiveLock();
  return ok;
}

bool FilesRmdir(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesMounted() && FilesPathIsValid(path) && FilesBackendRmdir(path);
  FilesGiveLock();
  return ok;
}

bool FilesStat(const char* path, HxFileInfo* out_info) {
  FilesFillEmptyInfo(out_info, path);

  if (!out_info || !g_files_ready || !FilesTakeLock()) {
    return false;
  }

  if (!FilesMounted() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return false;
  }

  bool is_dir    = false;
  size_t size    = 0;
  bool ok = FilesBackendStat(path, &is_dir, &size);

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
  out_info->mounted         = FilesMounted();
  out_info->partition_label = HX_FILES_PARTITION_LABEL;

  if (!FilesTakeLock()) {
    return false;
  }

  if (FilesMounted()) {
    size_t total = 0;
    size_t used  = 0;
    FilesBackendGetStorageInfo(&total, &used);
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

  if (!FilesMounted() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return false;
  }

  FilesListBridge bridge = { callback, user };
  bool ok = FilesBackendList(path,
                             reinterpret_cast<FilesBackendListCallback>(FilesListBridgeCb),
                             &bridge);
  FilesGiveLock();
  return ok;
}

String FilesReadText(const char* path) {
  String out;

  if (!g_files_ready || !FilesTakeLock()) {
    return out;
  }

  if (!FilesMounted() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return out;
  }

  // Determine size first, then read into heap buffer, convert to String.
  bool is_dir  = false;
  size_t fsize = 0;
  if (!FilesBackendStat(path, &is_dir, &fsize) || is_dir) {
    FilesGiveLock();
    return out;
  }

  if (fsize == 0) {
    FilesGiveLock();
    return out;
  }

  uint8_t* buf = static_cast<uint8_t*>(malloc(fsize + 1));
  if (!buf) {
    FilesGiveLock();
    return out;
  }

  size_t read_len = 0;
  if (FilesBackendReadBytes(path, buf, fsize, &read_len) && read_len > 0) {
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

  if (!FilesMounted() || !FilesPathIsValid(path)) {
    FilesGiveLock();
    return false;
  }

  bool ok = FilesBackendReadBytes(path, out_data, out_size, out_len);
  FilesGiveLock();
  return ok;
}

bool FilesWriteText(const char* path, const char* text) {
  const char* safe_text = text ? text : "";
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesInternal(path,
                                    reinterpret_cast<const uint8_t*>(safe_text),
                                    strlen(safe_text),
                                    false);
  FilesGiveLock();
  return ok;
}

bool FilesAppendText(const char* path, const char* text) {
  const char* safe_text = text ? text : "";
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesInternal(path,
                                    reinterpret_cast<const uint8_t*>(safe_text),
                                    strlen(safe_text),
                                    true);
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
  const char* safe_text = text ? text : "";
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }
  bool ok = FilesWriteBytesAtomicInternal(path,
                                          reinterpret_cast<const uint8_t*>(safe_text),
                                          strlen(safe_text));
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

#endif // HX_ENABLE_MODULE_STORAGE
