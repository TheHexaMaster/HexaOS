/*
  HexaOS - time_sync_service.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Time synchronization service API for HexaOS.
  Owns time synchronization policy across all sources: RTC boot read, NTP
  apply, source priority, and optional RTC write-back. Designed to be called
  from the boot orchestrator as an early-boot service hook and from runtime
  network services when a time source becomes available.

  Architecture
  This service is the policy owner for time synchronization. It is not a core
  component. Core (system/core/time) remains the sole source of truth for the
  canonical system time. This service feeds into core time via TimeSetFromRtc()
  and TimeSetFromNtp() and must never bypass those APIs.

  Dependency direction: service -> core/time, drivers/rtc_*, adapters/i2c_*
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Boot-phase API
// Called from BootInit() before ModuleInitAll(). Does not depend on modules.
// ---------------------------------------------------------------------------

// Attempt to read time from a configured RTC driver and apply it to core time.
// Safe to call even when no RTC is configured or available.
// Returns true if a valid time was obtained and applied.
bool TimeSyncBootTryRtc();

// ---------------------------------------------------------------------------
// Runtime API
// Called by network services or other time sources once available.
// ---------------------------------------------------------------------------

// Apply an NTP-sourced unix timestamp (seconds) to core time.
// Optionally writes back to a configured RTC if write-back is enabled.
// Returns true if time was successfully applied to core time.
bool TimeSyncApplyNtp(uint64_t unix_seconds);
