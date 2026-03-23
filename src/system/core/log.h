/*
  HexaOS - log.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central logging backend for HexaOS.
  Implements formatted log output, log level filtering, in-memory history
  buffering and synchronized sink-safe printing so log lines do not break
  interactive shell input while the active output transport remains pluggable.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

enum HxLogLevel : uint8_t {
  HX_LOG_ERROR = 0,
  HX_LOG_WARN  = 1,
  HX_LOG_INFO  = 2,
  HX_LOG_DEBUG = 3,
  HX_LOG_LLD   = 4   // Low Level Debug: hardware, bus, register, pin-level traces
};

typedef void (*HxLogSinkLineHook)();

typedef struct {
  size_t (*write_data)(const uint8_t* data, size_t len);
  size_t (*write_text)(const char* text);
  size_t (*write_char)(char ch);
  void (*flush)();
} HxLogSinkWriteOps;

void LogInit();
void LogSetSinkWriteOps(const HxLogSinkWriteOps* ops);
void LogSetLevel(HxLogLevel level);
HxLogLevel LogGetLevel();
void LogSetSinkLineHooks(HxLogSinkLineHook pre_write_line, HxLogSinkLineHook post_write_line);

void LogRaw(const char* text);
void LogDebug(const char* fmt, ...);
void LogInfo(const char* fmt, ...);
void LogWarn(const char* fmt, ...);
void LogError(const char* fmt, ...);
void LogTagged(HxLogLevel level, const char* tag, const char* fmt, ...);

size_t LogHistorySize();
size_t LogHistoryCapacity();
size_t LogHistoryCopy(char* out, size_t out_size);
void LogHistoryClear();
uint32_t LogDroppedLines();
uint32_t LogDroppedIsr();

void LogSinkWriteRaw(const char* text);
void LogSinkWriteChar(char ch);
void LogSinkWriteLineRaw(const char* text);

#define HX_LOGD(tag, fmt, ...)  LogTagged(HX_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define HX_LOGI(tag, fmt, ...)  LogTagged(HX_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define HX_LOGW(tag, fmt, ...)  LogTagged(HX_LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define HX_LOGE(tag, fmt, ...)  LogTagged(HX_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define HX_LOGLL(tag, fmt, ...) LogTagged(HX_LOG_LLD,   tag, fmt, ##__VA_ARGS__)