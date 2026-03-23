/*
  HexaOS - panic.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Fatal panic handler API for HexaOS.
  Provides structured panic codes, source-location-aware macros, and the
  last-resort halt or restart path for unrecoverable failures.

  Usage
  Prefer the HX_PANIC() macro over calling PanicAt() directly, as it
  automatically captures the source file and line number.

    HX_PANIC(HX_PANIC_BOOT, "Config NVS init failed");
    HX_PANIC_IF(!ok, HX_PANIC_ASSERT, "Invariant violated");

  Panic action (halt vs restart) is selected at build time via HX_PANIC_ACTION
  in hx_build.h.
*/

#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// Panic reason codes
// ---------------------------------------------------------------------------

typedef enum : uint8_t {
  HX_PANIC_UNKNOWN  = 0,  // Unclassified panic
  HX_PANIC_BOOT     = 1,  // Mandatory boot step failed fatally
  HX_PANIC_NVS      = 2,  // NVS backend unrecoverable error
  HX_PANIC_ASSERT   = 3,  // Explicit assertion failure
  HX_PANIC_MEMORY   = 4,  // Out of memory or memory corruption
  HX_PANIC_HARDWARE = 5,  // Unrecoverable hardware fault
} HxPanicCode;

// ---------------------------------------------------------------------------
// Internal implementation (do not call directly — use macros below)
// ---------------------------------------------------------------------------

void PanicAt(HxPanicCode code, const char* reason, const char* file, int line);

// ---------------------------------------------------------------------------
// Public API — preferred entry points
// ---------------------------------------------------------------------------

// Trigger a panic with source location captured automatically.
#define HX_PANIC(code, reason) \
  PanicAt((code), (reason), __FILE__, __LINE__)

// Trigger a panic only if the condition is true.
#define HX_PANIC_IF(cond, code, reason) \
  do { if (cond) { HX_PANIC(code, reason); } } while (0)

// ---------------------------------------------------------------------------
// Legacy compatibility
// Maps to HX_PANIC_UNKNOWN with no source location.
// Prefer HX_PANIC() in new code.
// ---------------------------------------------------------------------------

void Panic(const char* reason);
