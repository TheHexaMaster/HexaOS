/*
  HexaOS - littlefs_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Raw LittleFS backend adapter implementation.
  Sole owner of the LittleFS Arduino library dependency in HexaOS.
  All functions operate directly on the Arduino LittleFS object with no
  mutual exclusion — callers are responsible for serialising access.
*/

#include "littlefs_adapter.h"

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_LITTLEFS

#include <FS.h>
#include <LittleFS.h>
#include <stddef.h>
#include <string.h>

bool LfsInit(const char* partition_label) {
  (void)partition_label;
  return true;
}

bool LfsMount(const char* partition_label) {
  return LittleFS.begin(true, "", 10, partition_label);
}

bool LfsUnmount() {
  LittleFS.end();
  return true;
}

bool LfsFormat(const char* partition_label) {
  LittleFS.end();
  if (!LittleFS.format()) {
    return false;
  }
  return LittleFS.begin(true, "", 10, partition_label);
}

bool LfsExists(const char* path) {
  return LittleFS.exists(path);
}

bool LfsRemove(const char* path) {
  return LittleFS.remove(path);
}

bool LfsRename(const char* old_path, const char* new_path) {
  return LittleFS.rename(old_path, new_path);
}

bool LfsMkdir(const char* path) {
  return LittleFS.mkdir(path);
}

bool LfsRmdir(const char* path) {
  return LittleFS.rmdir(path);
}

bool LfsStat(const char* path, bool* out_is_dir, size_t* out_size) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  bool is_dir = file.isDirectory();
  if (out_is_dir) {
    *out_is_dir = is_dir;
  }
  if (out_size) {
    *out_size = is_dir ? 0 : (size_t)file.size();
  }

  file.close();
  return true;
}

bool LfsGetStorageInfo(size_t* out_total, size_t* out_used) {
  if (out_total) {
    *out_total = (size_t)LittleFS.totalBytes();
  }
  if (out_used) {
    *out_used = (size_t)LittleFS.usedBytes();
  }
  return true;
}

bool LfsList(const char* path, LfsListCallback callback, void* user) {
  if (!callback) {
    return false;
  }

  File root = LittleFS.open(path, "r");
  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    return false;
  }

  File entry = root.openNextFile();
  while (entry) {
    bool is_dir = entry.isDirectory();
    size_t size_bytes = is_dir ? 0 : (size_t)entry.size();
    const char* name = entry.name() ? entry.name() : "";

    bool keep_going = callback(name, is_dir, size_bytes, user);
    entry.close();

    if (!keep_going) {
      root.close();
      return true;
    }

    entry = root.openNextFile();
  }

  root.close();
  return true;
}

bool LfsReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len) {
  if (out_len) {
    *out_len = 0;
  }

  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return false;
  }

  size_t file_size = (size_t)file.size();

  if (file_size > out_size) {
    file.close();
    return false;
  }

  // Seek to beginning: some Arduino LittleFS builds internally seek inside
  // size(), which would leave the position at EOF and cause file.read() to
  // return 0 bytes even for a non-empty file.
  file.seek(0);

  // Read up to out_size bytes. Using out_size (>= file_size) rather than
  // file_size makes this robust against inaccurate size() reporting.
  size_t read_len = file.read(out, out_size);
  file.close();

  if (out_len) {
    *out_len = read_len;
  }

  return true;
}

bool LfsWriteBytes(const char* path, const uint8_t* data, size_t len, bool append) {
  const char* mode = append ? "a" : "w";
  File file = LittleFS.open(path, mode);
  if (!file) {
    return false;
  }

  if (len > 0 && data) {
    size_t written = file.write(data, len);
    if (written != len) {
      file.close();
      return false;
    }
  }

  file.flush();
  file.close();
  return true;
}

#endif // HX_ENABLE_FEATURE_LITTLEFS
