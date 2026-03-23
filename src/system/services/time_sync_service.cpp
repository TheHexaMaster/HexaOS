/*
  HexaOS - time_sync_service.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Time synchronization service implementation for HexaOS.
  Stub implementation. Future work: connect RTC driver reads/writes and NTP
  apply path once drivers/rtc_ds3232 and network stack are available.

  Boot hook (TimeSyncBootTryRtc) must be called from BootInit() after
  ConfigInit() and PinmapInit() so that driver bus bindings are resolved
  before attempting RTC communication.
*/

#include "time_sync_service.h"

#include "system/core/log.h"
#include "system/core/time.h"

bool TimeSyncBootTryRtc() {
  // TODO: resolve configured RTC driver from pinmap bindings
  // TODO: call Ds3232ReadUnixSeconds() or equivalent
  // TODO: validate result and call TimeSetFromRtc(unix_seconds)
  HX_LOGD("TSS", "boot RTC sync not yet implemented");
  return false;
}

bool TimeSyncApplyNtp(uint64_t unix_seconds) {
  if (unix_seconds == 0) {
    HX_LOGW("TSS", "NTP apply rejected: zero timestamp");
    return false;
  }

  if (!TimeSetUnixSeconds(unix_seconds, HX_TIME_SOURCE_NTP)) {
    HX_LOGW("TSS", "NTP apply failed: TimeSetUnixSeconds returned false");
    return false;
  }

  HX_LOGI("TSS", "NTP time applied unix=%llu", (unsigned long long)unix_seconds);

  // TODO: write back to RTC if write-back is enabled in config
  return true;
}
