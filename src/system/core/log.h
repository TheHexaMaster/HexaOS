/*
  HexaOS - log.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central logging backend for HexaOS.
  Implements formatted log output, log level filtering, in-memory history buffering and synchronized console-safe printing so log lines do not break interactive shell input.
*/

#pragma once

enum HxLogLevel : uint8_t {
  HX_LOG_ERROR = 0,
  HX_LOG_WARN  = 1,
  HX_LOG_INFO  = 2,
  HX_LOG_DEBUG = 3
};

void LogInit();
void LogSetLevel(HxLogLevel level);
HxLogLevel LogGetLevel();
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

#define HX_LOGD(tag, fmt, ...) LogTagged(HX_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define HX_LOGI(tag, fmt, ...) LogTagged(HX_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define HX_LOGW(tag, fmt, ...) LogTagged(HX_LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define HX_LOGE(tag, fmt, ...) LogTagged(HX_LOG_ERROR, tag, fmt, ##__VA_ARGS__)