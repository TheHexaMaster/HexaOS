/*
  HexaOS - littlefs_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  High-level filesystem service.
  Exposes simple file operations for mounted LittleFS storage and isolates file existence, read and write helpers from direct framework usage.
*/
#include "hexaos.h"

#if HX_ENABLE_HANDLER_LITTLEFS

#include <FS.h>
#include <LittleFS.h>

bool FilesInit() {
  return true;
}

bool FilesMount() {
  Hx.littlefs_mounted = LittleFS.begin(true, "", 10, "littlefs");

  if (Hx.littlefs_mounted) {
    LogInfo("LittleFS mount OK");
  } else {
    LogError("LittleFS mount failed");
  }

  return Hx.littlefs_mounted;
}

bool FilesExists(const char* path) {
  if (!Hx.littlefs_mounted || !path) {
    return false;
  }

  return LittleFS.exists(path);
}

String FilesReadText(const char* path) {
  if (!Hx.littlefs_mounted || !path) {
    return String();
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    return String();
  }

  String out = f.readString();
  f.close();
  return out;
}

bool FilesWriteText(const char* path, const char* text) {
  if (!Hx.littlefs_mounted || !path) {
    return false;
  }

  File f = LittleFS.open(path, "w");
  if (!f) {
    return false;
  }

  f.print(text ? text : "");
  f.close();
  return true;
}



#endif // HX_ENABLE_HANDLER_LITTLEFS