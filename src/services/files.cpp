#include "hexaos.h"
#include <FS.h>
#include <LittleFS.h>

bool FilesInit() {
  return true;
}

bool FilesMount() {
  Hx.littlefs_mounted = EspLittlefsMount();
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
