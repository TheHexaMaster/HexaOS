/*
  HexaOS - littlefs_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  High-level filesystem service for the LittleFS backend.
  Provides synchronized mount, format, metadata, directory listing and
  text/binary file operations so the rest of HexaOS does not need to access
  Arduino filesystem primitives directly.
*/

#include "headers/hx_build.h"
#include "system/core/log.h"
#include "system/core/rtos.h"
#include "system/core/runtime.h"
#include "system/handlers/littlefs_handler.h"

#if HX_ENABLE_HANDLER_LITTLEFS

#include <FS.h>
#include <LittleFS.h>

#include <stdio.h>
#include <string.h>

static constexpr const char* HX_FILES_TAG = "FIL";
static constexpr const char* HX_FILES_PARTITION_LABEL = "littlefs";
static constexpr size_t HX_FILES_PATH_MAX = 255;

static bool g_files_ready = false;
static HxRtosCritical g_files_state_lock = HX_RTOS_CRITICAL_INIT;
static HxRtosMutex g_files_mutex = HX_RTOS_MUTEX_INIT;

static void FilesSetMounted(bool mounted) {
  RtosCriticalEnter(&g_files_state_lock);
  Hx.littlefs_mounted = mounted;
  RtosCriticalExit(&g_files_state_lock);
}

static bool FilesMounted() {
  bool mounted = false;
  RtosCriticalEnter(&g_files_state_lock);
  mounted = Hx.littlefs_mounted;
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

static bool FilesOpenForRead(const char* path, File* out_file) {
  if (!out_file || !FilesMounted() || !FilesPathIsValid(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  *out_file = file;
  return true;
}

static bool FilesWriteBytesInternal(const char* path, const uint8_t* data, size_t len, bool append_mode) {
  if (!FilesMounted() || !FilesPathIsValid(path)) {
    return false;
  }

  if (len > 0 && !data) {
    return false;
  }

  const char* mode = append_mode ? "a" : "w";
  File file = LittleFS.open(path, mode);
  if (!file) {
    HX_LOGW(HX_FILES_TAG, "open failed path=%s mode=%s", path, mode);
    return false;
  }

  if (len > 0) {
    size_t written = file.write(data, len);
    if (written != len) {
      file.close();
      HX_LOGW(HX_FILES_TAG, "write failed path=%s written=%u expected=%u",
              path,
              (unsigned)written,
              (unsigned)len);
      return false;
    }
  }

  file.flush();
  file.close();
  return true;
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

  LittleFS.remove(temp_path);

  if (!FilesWriteBytesInternal(temp_path, data, len, false)) {
    LittleFS.remove(temp_path);
    return false;
  }

  if (LittleFS.exists(path) && !LittleFS.remove(path)) {
    LittleFS.remove(temp_path);
    HX_LOGW(HX_FILES_TAG, "remove before rename failed path=%s", path);
    return false;
  }

  if (!LittleFS.rename(temp_path, path)) {
    LittleFS.remove(temp_path);
    HX_LOGW(HX_FILES_TAG, "atomic rename failed path=%s", path);
    return false;
  }

  return true;
}

bool FilesInit() {
  if (g_files_ready) {
    return true;
  }

  if (!RtosCriticalInit(&g_files_state_lock)) {
    HX_LOGE(HX_FILES_TAG, "init failed state lock");
    return false;
  }

  if (!RtosMutexInit(&g_files_mutex)) {
    HX_LOGE(HX_FILES_TAG, "init failed mutex");
    RtosCriticalDestroy(&g_files_state_lock);
    return false;
  }

  FilesSetMounted(false);
  g_files_ready = true;
  HX_LOGI(HX_FILES_TAG, "init OK partition=%s", HX_FILES_PARTITION_LABEL);
  return true;
}

bool FilesMount() {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool mounted = LittleFS.begin(true, "", 10, HX_FILES_PARTITION_LABEL);
  FilesSetMounted(mounted);

  if (mounted) {
    HX_LOGI(HX_FILES_TAG, "mount OK partition=%s total=%u used=%u",
            HX_FILES_PARTITION_LABEL,
            (unsigned)LittleFS.totalBytes(),
            (unsigned)LittleFS.usedBytes());
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
    LittleFS.end();
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

  if (FilesMounted()) {
    LittleFS.end();
    FilesSetMounted(false);
  }

  bool formatted = LittleFS.format();
  bool mounted = false;
  if (formatted) {
    mounted = LittleFS.begin(true, "", 10, HX_FILES_PARTITION_LABEL);
    FilesSetMounted(mounted);
  }

  if (formatted && mounted) {
    HX_LOGW(HX_FILES_TAG, "format OK partition=%s", HX_FILES_PARTITION_LABEL);
  } else if (formatted) {
    HX_LOGE(HX_FILES_TAG, "format OK but remount failed partition=%s", HX_FILES_PARTITION_LABEL);
  } else {
    HX_LOGE(HX_FILES_TAG, "format failed partition=%s", HX_FILES_PARTITION_LABEL);
  }

  FilesGiveLock();
  return (formatted && mounted);
}

bool FilesExists(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool exists = FilesMounted() && FilesPathIsValid(path) && LittleFS.exists(path);
  FilesGiveLock();
  return exists;
}

bool FilesRemove(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool ok = FilesMounted() && FilesPathIsValid(path) && LittleFS.remove(path);
  FilesGiveLock();
  return ok;
}

bool FilesRename(const char* old_path, const char* new_path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool ok = FilesMounted() && FilesPathIsValid(old_path) && FilesPathIsValid(new_path) && LittleFS.rename(old_path, new_path);
  FilesGiveLock();
  return ok;
}

bool FilesMkdir(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool ok = FilesMounted() && FilesPathIsValid(path) && LittleFS.mkdir(path);
  FilesGiveLock();
  return ok;
}

bool FilesRmdir(const char* path) {
  if (!g_files_ready || !FilesTakeLock()) {
    return false;
  }

  bool ok = FilesMounted() && FilesPathIsValid(path) && LittleFS.rmdir(path);
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

  File file = LittleFS.open(path, "r");
  if (!file) {
    FilesGiveLock();
    return false;
  }

  out_info->exists = true;
  out_info->is_dir = file.isDirectory();
  out_info->size_bytes = out_info->is_dir ? 0 : (size_t)file.size();
  snprintf(out_info->path, sizeof(out_info->path), "%s", path);
  file.close();

  FilesGiveLock();
  return true;
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
  out_info->ready = g_files_ready;
  out_info->mounted = FilesMounted();
  out_info->partition_label = HX_FILES_PARTITION_LABEL;

  if (!FilesTakeLock()) {
    return false;
  }

  if (FilesMounted()) {
    out_info->total_bytes = (size_t)LittleFS.totalBytes();
    out_info->used_bytes = (size_t)LittleFS.usedBytes();
    out_info->free_bytes = (out_info->total_bytes >= out_info->used_bytes)
                             ? (out_info->total_bytes - out_info->used_bytes)
                             : 0;
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

  File root = LittleFS.open(path, "r");
  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    FilesGiveLock();
    return false;
  }

  File entry = root.openNextFile();
  while (entry) {
    HxFileInfo info = {};
    snprintf(info.path, sizeof(info.path), "%s", entry.name() ? entry.name() : "");
    info.exists = true;
    info.is_dir = entry.isDirectory();
    info.size_bytes = info.is_dir ? 0 : (size_t)entry.size();

    bool keep_going = callback(&info, user);
    entry.close();
    if (!keep_going) {
      root.close();
      FilesGiveLock();
      return true;
    }

    entry = root.openNextFile();
  }

  root.close();
  FilesGiveLock();
  return true;
}

String FilesReadText(const char* path) {
  String out;

  if (!g_files_ready || !FilesTakeLock()) {
    return out;
  }

  File file;
  if (!FilesOpenForRead(path, &file) || file.isDirectory()) {
    if (file) {
      file.close();
    }
    FilesGiveLock();
    return out;
  }

  size_t file_size = (size_t)file.size();
  if (file_size > 0) {
    out.reserve(file_size + 1);
  }

  out = file.readString();
  file.close();
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

  File file;
  if (!FilesOpenForRead(path, &file) || file.isDirectory()) {
    if (file) {
      file.close();
    }
    FilesGiveLock();
    return false;
  }

  size_t file_size = (size_t)file.size();
  if (out_len) {
    *out_len = file_size;
  }

  if (file_size > out_size) {
    file.close();
    FilesGiveLock();
    return false;
  }

  size_t read_len = file.read(out_data, file_size);
  file.close();
  FilesGiveLock();

  if (out_len) {
    *out_len = read_len;
  }

  return (read_len == file_size);
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

#endif // HX_ENABLE_HANDLER_LITTLEFS
