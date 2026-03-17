#include "hexaos.h"

static void VLog(const char* level, const char* fmt, va_list ap) {
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, ap);

  Serial.print("[");
  Serial.print(level);
  Serial.print("] ");
  Serial.println(buf);
}

void LogInit() {
  Serial.begin(115200);
  delay(50);
}

void LogRaw(const char* text) {
  Serial.println(text);
}

void LogInfo(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VLog("INF", fmt, ap);
  va_end(ap);
}

void LogWarn(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VLog("WRN", fmt, ap);
  va_end(ap);
}

void LogError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VLog("ERR", fmt, ap);
  va_end(ap);
}