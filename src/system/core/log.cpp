/*
  HexaOS - log.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central logging backend for HexaOS.
  Implements formatted log output, log level filtering, in-memory history
  buffering and synchronized console-safe printing so log lines do not break
  interactive shell input.
*/

#include "hexaos.h"
#include "system/adapters/console_adapter.h"

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static constexpr size_t HX_LOG_LINE_MAX = 256;
static constexpr size_t HX_LOG_HISTORY_BYTES = 8192;

static portMUX_TYPE g_log_state_mux = portMUX_INITIALIZER_UNLOCKED;
static StaticSemaphore_t g_log_sink_mutex_buf;
static SemaphoreHandle_t g_log_sink_mutex = nullptr;

static volatile HxLogLevel g_log_level = HX_LOG_INFO;
static char g_log_history[HX_LOG_HISTORY_BYTES];
static size_t g_log_head = 0;
static size_t g_log_tail = 0;
static size_t g_log_used = 0;
static uint32_t g_log_dropped_lines = 0;
static uint32_t g_log_dropped_isr = 0;
static HxLogSinkLineHook g_log_sink_pre_line_hook = nullptr;
static HxLogSinkLineHook g_log_sink_post_line_hook = nullptr;

static void LogGetSinkLineHooks(HxLogSinkLineHook* pre_write_line, HxLogSinkLineHook* post_write_line) {
  taskENTER_CRITICAL(&g_log_state_mux);

  if (pre_write_line) {
    *pre_write_line = g_log_sink_pre_line_hook;
  }

  if (post_write_line) {
    *post_write_line = g_log_sink_post_line_hook;
  }

  taskEXIT_CRITICAL(&g_log_state_mux);
}

static const char* LogLevelText(HxLogLevel level) {
  switch (level) {
    case HX_LOG_ERROR: return "ERR";
    case HX_LOG_WARN:  return "WRN";
    case HX_LOG_INFO:  return "INF";
    case HX_LOG_DEBUG: return "DBG";
    default:           return "UNK";
  }
}

static bool LogIsInIsr() {
  return xPortInIsrContext();
}

static void LogRecordIsrDrop() {
  taskENTER_CRITICAL_ISR(&g_log_state_mux);
  g_log_dropped_isr++;
  taskEXIT_CRITICAL_ISR(&g_log_state_mux);
}

static bool LogShouldWrite(HxLogLevel level) {
  HxLogLevel current;
  taskENTER_CRITICAL(&g_log_state_mux);
  current = g_log_level;
  taskEXIT_CRITICAL(&g_log_state_mux);
  return level <= current;
}

static bool LogTakeSinkLock() {
  if (!g_log_sink_mutex) {
    return true;
  }

  if (LogIsInIsr()) {
    return false;
  }

  return (xSemaphoreTake(g_log_sink_mutex, portMAX_DELAY) == pdTRUE);
}

static void LogGiveSinkLock() {
  if (!g_log_sink_mutex) {
    return;
  }

  if (LogIsInIsr()) {
    return;
  }

  xSemaphoreGive(g_log_sink_mutex);
}

static void LogHistoryDropOldestByteLocked() {
  if (g_log_used == 0) {
    return;
  }

  char dropped = g_log_history[g_log_tail];
  g_log_tail = (g_log_tail + 1) % HX_LOG_HISTORY_BYTES;
  g_log_used--;

  if (dropped == '\n') {
    g_log_dropped_lines++;
  }
}

static void LogHistoryAppendLocked(const char* text, size_t len) {
  if (!text || (len == 0)) {
    return;
  }

  if (len > HX_LOG_HISTORY_BYTES) {
    text += (len - HX_LOG_HISTORY_BYTES);
    len = HX_LOG_HISTORY_BYTES;
  }

  while ((g_log_used + len) > HX_LOG_HISTORY_BYTES) {
    LogHistoryDropOldestByteLocked();
  }

  size_t first = len;
  size_t space_to_end = HX_LOG_HISTORY_BYTES - g_log_head;
  if (first > space_to_end) {
    first = space_to_end;
  }

  memcpy(&g_log_history[g_log_head], text, first);

  size_t second = len - first;
  if (second > 0) {
    memcpy(&g_log_history[0], text + first, second);
  }

  g_log_head = (g_log_head + len) % HX_LOG_HISTORY_BYTES;
  g_log_used += len;
}

static void LogStoreLine(const char* line) {
  if (!line) {
    return;
  }

  taskENTER_CRITICAL(&g_log_state_mux);
  LogHistoryAppendLocked(line, strlen(line));
  LogHistoryAppendLocked("\n", 1);
  taskEXIT_CRITICAL(&g_log_state_mux);
}

static void LogEmitLine(const char* line) {
  if (!line) {
    return;
  }

  if (LogIsInIsr()) {
    LogRecordIsrDrop();
    return;
  }

  if (LogTakeSinkLock()) {
    HxLogSinkLineHook pre_hook = nullptr;
    HxLogSinkLineHook post_hook = nullptr;

    LogGetSinkLineHooks(&pre_hook, &post_hook);

    if (pre_hook) {
      pre_hook();
    }

    ConsoleAdapterWriteText(line);
    ConsoleAdapterWriteText("\r\n");
    ConsoleAdapterFlush();
    LogStoreLine(line);

    if (post_hook) {
      post_hook();
    }

    LogGiveSinkLock();
    return;
  }

  LogStoreLine(line);
}

void LogSinkWriteRaw(const char* text) {
  if (LogIsInIsr()) {
    LogRecordIsrDrop();
    return;
  }

  if (!text) {
    text = "";
  }

  if (LogTakeSinkLock()) {
    ConsoleAdapterWriteText(text);
    LogGiveSinkLock();
  }
}

void LogSinkWriteChar(char ch) {
  if (LogIsInIsr()) {
    LogRecordIsrDrop();
    return;
  }

  if (LogTakeSinkLock()) {
    ConsoleAdapterWriteChar(ch);
    LogGiveSinkLock();
  }
}

void LogSinkWriteLineRaw(const char* text) {
  if (LogIsInIsr()) {
    LogRecordIsrDrop();
    return;
  }

  if (!text) {
    text = "";
  }

  if (LogTakeSinkLock()) {
    HxLogSinkLineHook pre_hook = nullptr;
    HxLogSinkLineHook post_hook = nullptr;

    LogGetSinkLineHooks(&pre_hook, &post_hook);

    if (pre_hook) {
      pre_hook();
    }

    ConsoleAdapterWriteText(text);
    ConsoleAdapterWriteText("\r\n");
    ConsoleAdapterFlush();

    if (post_hook) {
      post_hook();
    }

    LogGiveSinkLock();
  }
}

static void LogWriteV(HxLogLevel level, const char* tag, const char* fmt, va_list ap) {
  if (!fmt) {
    return;
  }

  if (LogIsInIsr()) {
    LogRecordIsrDrop();
    return;
  }

  if (!LogShouldWrite(level)) {
    return;
  }

  char line[HX_LOG_LINE_MAX];
  uint32_t now = millis();

  int prefix_len = 0;
  if (tag && tag[0]) {
    prefix_len = snprintf(line, sizeof(line), "[%8lu][%s][%s] ",
                          (unsigned long)now,
                          LogLevelText(level),
                          tag);
  } else {
    prefix_len = snprintf(line, sizeof(line), "[%8lu][%s] ",
                          (unsigned long)now,
                          LogLevelText(level));
  }

  if (prefix_len < 0) {
    return;
  }

  size_t offset = (size_t)prefix_len;
  if (offset >= sizeof(line)) {
    offset = sizeof(line) - 1;
  }

  vsnprintf(line + offset, sizeof(line) - offset, fmt, ap);
  line[sizeof(line) - 1] = '\0';

  LogEmitLine(line);
}

void LogInit() {
  ConsoleAdapterInit();

  if (!g_log_sink_mutex) {
    g_log_sink_mutex = xSemaphoreCreateMutexStatic(&g_log_sink_mutex_buf);
  }

  LogHistoryClear();

  HxLogLevel level = (HxLogLevel)HX_BUILD_DEFAULT_LOG_LEVEL;
  if ((level < HX_LOG_ERROR) || (level > HX_LOG_DEBUG)) {
    level = HX_LOG_INFO;
  }

  LogSetLevel(level);
}

void LogSetSinkLineHooks(HxLogSinkLineHook pre_write_line, HxLogSinkLineHook post_write_line) {
  taskENTER_CRITICAL(&g_log_state_mux);
  g_log_sink_pre_line_hook = pre_write_line;
  g_log_sink_post_line_hook = post_write_line;
  taskEXIT_CRITICAL(&g_log_state_mux);
}

void LogSetLevel(HxLogLevel level) {
  taskENTER_CRITICAL(&g_log_state_mux);
  g_log_level = level;
  taskEXIT_CRITICAL(&g_log_state_mux);
}

HxLogLevel LogGetLevel() {
  HxLogLevel level;
  taskENTER_CRITICAL(&g_log_state_mux);
  level = g_log_level;
  taskEXIT_CRITICAL(&g_log_state_mux);
  return level;
}

void LogRaw(const char* text) {
  LogEmitLine(text ? text : "");
}

void LogDebug(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  LogWriteV(HX_LOG_DEBUG, nullptr, fmt, ap);
  va_end(ap);
}

void LogInfo(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  LogWriteV(HX_LOG_INFO, nullptr, fmt, ap);
  va_end(ap);
}

void LogWarn(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  LogWriteV(HX_LOG_WARN, nullptr, fmt, ap);
  va_end(ap);
}

void LogError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  LogWriteV(HX_LOG_ERROR, nullptr, fmt, ap);
  va_end(ap);
}

void LogTagged(HxLogLevel level, const char* tag, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  LogWriteV(level, tag, fmt, ap);
  va_end(ap);
}

size_t LogHistorySize() {
  size_t used;
  taskENTER_CRITICAL(&g_log_state_mux);
  used = g_log_used;
  taskEXIT_CRITICAL(&g_log_state_mux);
  return used;
}

size_t LogHistoryCapacity() {
  return HX_LOG_HISTORY_BYTES;
}

size_t LogHistoryCopy(char* out, size_t out_size) {
  if (!out || (out_size == 0)) {
    return 0;
  }

  out[0] = '\0';

  taskENTER_CRITICAL(&g_log_state_mux);

  if (g_log_used == 0) {
    taskEXIT_CRITICAL(&g_log_state_mux);
    return 0;
  }

  size_t copy_len = g_log_used;
  if (copy_len > (out_size - 1)) {
    copy_len = out_size - 1;
  }

  size_t skip = g_log_used - copy_len;
  size_t idx = (g_log_tail + skip) % HX_LOG_HISTORY_BYTES;

  size_t first = copy_len;
  size_t bytes_to_end = HX_LOG_HISTORY_BYTES - idx;
  if (first > bytes_to_end) {
    first = bytes_to_end;
  }

  memcpy(out, &g_log_history[idx], first);

  size_t second = copy_len - first;
  if (second > 0) {
    memcpy(out + first, &g_log_history[0], second);
  }

  out[copy_len] = '\0';

  taskEXIT_CRITICAL(&g_log_state_mux);
  return copy_len;
}

void LogHistoryClear() {
  taskENTER_CRITICAL(&g_log_state_mux);
  memset(g_log_history, 0, sizeof(g_log_history));
  g_log_head = 0;
  g_log_tail = 0;
  g_log_used = 0;
  g_log_dropped_lines = 0;
  g_log_dropped_isr = 0;
  taskEXIT_CRITICAL(&g_log_state_mux);
}

uint32_t LogDroppedLines() {
  uint32_t dropped;
  taskENTER_CRITICAL(&g_log_state_mux);
  dropped = g_log_dropped_lines;
  taskEXIT_CRITICAL(&g_log_state_mux);
  return dropped;
}

uint32_t LogDroppedIsr() {
  uint32_t dropped;
  taskENTER_CRITICAL(&g_log_state_mux);
  dropped = g_log_dropped_isr;
  taskEXIT_CRITICAL(&g_log_state_mux);
  return dropped;
}