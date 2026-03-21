/*
  HexaOS - time.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central time engine implementation for HexaOS.
  Keeps a monotonic boot-time counter based on the ESP timer and an optional
  synchronized wall clock anchored by RTC, NTP or manual updates.
*/

#include "time.h"

#include <esp_timer.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct HxTimeState {
  bool ready;
  bool synchronized;
  HxTimeSource source;
  uint64_t sync_monotonic_ms;
  uint64_t sync_unix_ms;
};

static HxRtosCritical g_time_critical = HX_RTOS_CRITICAL_INIT;
static HxTimeState g_time_state = {
  .ready = false,
  .synchronized = false,
  .source = HX_TIME_SOURCE_NONE,
  .sync_monotonic_ms = 0,
  .sync_unix_ms = 0
};

static uint64_t TimeReadMonotonicNowMs() {
  int64_t us = esp_timer_get_time();
  if (us <= 0) {
    return 0;
  }

  return (uint64_t)us / 1000ULL;
}

static void TimeStateEnter() {
  RtosCriticalEnter(&g_time_critical);
}

static void TimeStateExit() {
  RtosCriticalExit(&g_time_critical);
}

bool TimeInit() {
  if (!RtosCriticalReady(&g_time_critical)) {
    if (!RtosCriticalInit(&g_time_critical)) {
      return false;
    }
  }

  TimeStateEnter();
  g_time_state.ready = true;
  g_time_state.synchronized = false;
  g_time_state.source = HX_TIME_SOURCE_NONE;
  g_time_state.sync_monotonic_ms = 0;
  g_time_state.sync_unix_ms = 0;
  TimeStateExit();

  return true;
}

bool TimeReady() {
  bool ready = false;

  TimeStateEnter();
  ready = g_time_state.ready;
  TimeStateExit();

  return ready;
}

uint64_t TimeMonotonicMs() {
  return TimeReadMonotonicNowMs();
}

uint32_t TimeMonotonicMs32() {
  return (uint32_t)TimeReadMonotonicNowMs();
}

bool TimeIsSynchronized() {
  bool synchronized = false;

  TimeStateEnter();
  synchronized = g_time_state.synchronized;
  TimeStateExit();

  return synchronized;
}

HxTimeSource TimeGetSource() {
  HxTimeSource source = HX_TIME_SOURCE_NONE;

  TimeStateEnter();
  source = g_time_state.source;
  TimeStateExit();

  return source;
}

const char* TimeSourceText(HxTimeSource source) {
  switch (source) {
    case HX_TIME_SOURCE_USER: return "user";
    case HX_TIME_SOURCE_RTC:  return "rtc";
    case HX_TIME_SOURCE_NTP:  return "ntp";
    case HX_TIME_SOURCE_NONE:
    default:                  return "none";
  }
}

bool TimeGetInfo(HxTimeInfo* out_info) {
  if (!out_info) {
    return false;
  }

  HxTimeState snapshot{};
  uint64_t now_ms = TimeReadMonotonicNowMs();

  TimeStateEnter();
  snapshot = g_time_state;
  TimeStateExit();

  out_info->ready = snapshot.ready;
  out_info->synchronized = snapshot.synchronized;
  out_info->source = snapshot.source;
  out_info->monotonic_ms = now_ms;
  out_info->sync_monotonic_ms = snapshot.sync_monotonic_ms;
  out_info->sync_unix_ms = snapshot.sync_unix_ms;

  if (snapshot.synchronized) {
    uint64_t delta_ms = now_ms - snapshot.sync_monotonic_ms;
    out_info->unix_ms = snapshot.sync_unix_ms + delta_ms;
    out_info->sync_age_ms = delta_ms;
  } else {
    out_info->unix_ms = 0;
    out_info->sync_age_ms = 0;
  }

  return true;
}

bool TimeSetUnixMs(uint64_t unix_ms, HxTimeSource source) {
  if (source == HX_TIME_SOURCE_NONE) {
    return false;
  }

  uint64_t now_ms = TimeReadMonotonicNowMs();

  TimeStateEnter();
  g_time_state.ready = true;
  g_time_state.synchronized = true;
  g_time_state.source = source;
  g_time_state.sync_monotonic_ms = now_ms;
  g_time_state.sync_unix_ms = unix_ms;
  TimeStateExit();

  return true;
}

bool TimeSetUnixSeconds(uint64_t unix_seconds, HxTimeSource source) {
  return TimeSetUnixMs(unix_seconds * 1000ULL, source);
}

bool TimeSetFromRtc(uint64_t unix_seconds) {
  return TimeSetUnixSeconds(unix_seconds, HX_TIME_SOURCE_RTC);
}

bool TimeSetFromNtp(uint64_t unix_seconds) {
  return TimeSetUnixSeconds(unix_seconds, HX_TIME_SOURCE_NTP);
}

void TimeClearSynchronization() {
  TimeStateEnter();
  g_time_state.synchronized = false;
  g_time_state.source = HX_TIME_SOURCE_NONE;
  g_time_state.sync_monotonic_ms = 0;
  g_time_state.sync_unix_ms = 0;
  TimeStateExit();
}

bool TimeGetUnixMs(uint64_t* out_unix_ms) {
  if (!out_unix_ms) {
    return false;
  }

  HxTimeInfo info{};
  if (!TimeGetInfo(&info) || !info.synchronized) {
    return false;
  }

  *out_unix_ms = info.unix_ms;
  return true;
}

bool TimeGetUnixSeconds(uint64_t* out_unix_seconds) {
  if (!out_unix_seconds) {
    return false;
  }

  uint64_t unix_ms = 0;
  if (!TimeGetUnixMs(&unix_ms)) {
    return false;
  }

  *out_unix_seconds = unix_ms / 1000ULL;
  return true;
}

bool TimeFormatUtc(char* out, size_t out_size, uint64_t unix_ms) {
  if (!out || (out_size == 0)) {
    return false;
  }

  time_t unix_seconds = (time_t)(unix_ms / 1000ULL);
  struct tm tm_utc{};
  if (!gmtime_r(&unix_seconds, &tm_utc)) {
    out[0] = '\0';
    return false;
  }

  int written = snprintf(out,
                         out_size,
                         "%04d-%02d-%02d %02d:%02d:%02d UTC",
                         tm_utc.tm_year + 1900,
                         tm_utc.tm_mon + 1,
                         tm_utc.tm_mday,
                         tm_utc.tm_hour,
                         tm_utc.tm_min,
                         tm_utc.tm_sec);

  if ((written < 0) || ((size_t)written >= out_size)) {
    if (out_size > 0) {
      out[out_size - 1] = '\0';
    }
    return false;
  }

  return true;
}

bool TimeFormatNowUtc(char* out, size_t out_size) {
  uint64_t unix_ms = 0;
  if (!TimeGetUnixMs(&unix_ms)) {
    if (out && (out_size > 0)) {
      out[0] = '\0';
    }
    return false;
  }

  return TimeFormatUtc(out, out_size, unix_ms);
}

void TimeFormatMonotonic(char* out, size_t out_size, uint64_t monotonic_ms) {
  if (!out || (out_size == 0)) {
    return;
  }

  uint64_t total_seconds = monotonic_ms / 1000ULL;
  uint64_t millis_part = monotonic_ms % 1000ULL;
  uint64_t days = total_seconds / 86400ULL;
  uint64_t hours = (total_seconds % 86400ULL) / 3600ULL;
  uint64_t minutes = (total_seconds % 3600ULL) / 60ULL;
  uint64_t seconds = total_seconds % 60ULL;

  if (days > 0) {
    snprintf(out,
             out_size,
             "%llud %02llu:%02llu:%02llu.%03llu",
             (unsigned long long)days,
             (unsigned long long)hours,
             (unsigned long long)minutes,
             (unsigned long long)seconds,
             (unsigned long long)millis_part);
  } else {
    snprintf(out,
             out_size,
             "%02llu:%02llu:%02llu.%03llu",
             (unsigned long long)hours,
             (unsigned long long)minutes,
             (unsigned long long)seconds,
             (unsigned long long)millis_part);
  }
}
