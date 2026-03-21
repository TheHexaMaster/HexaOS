/*
  HexaOS - time.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central time engine API for HexaOS.
  Provides one explicit interface for monotonic boot time and synchronized wall
  clock time so future RTC and NTP backends can update one shared source of
  truth without coupling the rest of the system to a specific provider.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "system/core/rtos.h"

enum HxTimeSource : uint8_t {
  HX_TIME_SOURCE_NONE = 0,
  HX_TIME_SOURCE_USER = 1,
  HX_TIME_SOURCE_RTC  = 2,
  HX_TIME_SOURCE_NTP  = 3
};

struct HxTimeInfo {
  bool ready;
  bool synchronized;
  HxTimeSource source;
  uint64_t monotonic_ms;
  uint64_t unix_ms;
  uint64_t sync_monotonic_ms;
  uint64_t sync_unix_ms;
  uint64_t sync_age_ms;
};

bool TimeInit();
bool TimeReady();

uint64_t TimeMonotonicMs();
uint32_t TimeMonotonicMs32();

bool TimeIsSynchronized();
HxTimeSource TimeGetSource();
const char* TimeSourceText(HxTimeSource source);

bool TimeGetInfo(HxTimeInfo* out_info);

bool TimeSetUnixMs(uint64_t unix_ms, HxTimeSource source);
bool TimeSetUnixSeconds(uint64_t unix_seconds, HxTimeSource source);
bool TimeSetFromRtc(uint64_t unix_seconds);
bool TimeSetFromNtp(uint64_t unix_seconds);
void TimeClearSynchronization();

bool TimeGetUnixMs(uint64_t* out_unix_ms);
bool TimeGetUnixSeconds(uint64_t* out_unix_seconds);

bool TimeFormatUtc(char* out, size_t out_size, uint64_t unix_ms);
bool TimeFormatNowUtc(char* out, size_t out_size);
bool TimeFormatLogStamp(char* out, size_t out_size);
bool TimeFormatUint64(char* out, size_t out_size, uint64_t value);
void TimeFormatMonotonic(char* out, size_t out_size, uint64_t monotonic_ms);
