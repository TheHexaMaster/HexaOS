/*
  HexaOS - panic.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Fatal panic handler implementation for HexaOS.
  Provides the last-resort stop path for unrecoverable failures. Writes a
  structured panic banner directly to the console adapter (defensive, works
  even before the log system is initialized) and also routes through the log
  system if available. After printing, the system either halts with periodic
  banner repeats or restarts, depending on the HX_PANIC_ACTION build selector.
*/

#include "panic.h"

#include <esp32-hal.h>
#include <esp_system.h>
#include <stdio.h>

#include "headers/hx_build.h"
#include "system/adapters/console_adapter.h"
#include "system/core/log.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static const char* PanicCodeText(HxPanicCode code) {
  switch (code) {
    case HX_PANIC_BOOT:     return "BOOT";
    case HX_PANIC_NVS:      return "NVS";
    case HX_PANIC_ASSERT:   return "ASSERT";
    case HX_PANIC_MEMORY:   return "MEMORY";
    case HX_PANIC_HARDWARE: return "HARDWARE";
    default:                return "UNKNOWN";
  }
}

// Writes the panic banner directly to the console adapter.
// Intentionally does not use the log system — must remain safe to call before
// LogInit() and in states where the log sink may itself be broken.
static void PanicWriteBanner(HxPanicCode code, const char* reason, const char* file, int line) {
  char buf[160];

  ConsoleAdapterWriteText("\r\n========================================\r\n");
  ConsoleAdapterWriteText("!! PANIC !! HexaOS halted\r\n");

  snprintf(buf, sizeof(buf), "  code:   %s (%d)\r\n", PanicCodeText(code), (int)code);
  ConsoleAdapterWriteText(buf);

  if (reason && reason[0]) {
    snprintf(buf, sizeof(buf), "  reason: %s\r\n", reason);
    ConsoleAdapterWriteText(buf);
  }

  if (file && file[0]) {
    snprintf(buf, sizeof(buf), "  at:     %s:%d\r\n", file, line);
    ConsoleAdapterWriteText(buf);
  }

  ConsoleAdapterWriteText("========================================\r\n");
  ConsoleAdapterFlush();
}

// ---------------------------------------------------------------------------
// Public implementation
// ---------------------------------------------------------------------------

void PanicAt(HxPanicCode code, const char* reason, const char* file, int line) {
  ConsoleAdapterInit();
  delay(10);

  // Route through the log system if it has been initialized.
  LogError("PANIC [%s]: %s (%s:%d)",
           PanicCodeText(code),
           reason ? reason : "",
           file    ? file   : "",
           line);

  // Always write the banner directly to the console as a defensive fallback.
  PanicWriteBanner(code, reason, file, line);

#if (HX_PANIC_ACTION == HX_PANIC_ACTION_RESTART)
  // Production action: restart after a short delay so the banner is visible.
  delay(3000);
  esp_restart();
#else
  // Debug action (default): halt and repeat the banner periodically so a
  // late-connecting serial monitor can still read the panic cause.
  while (true) {
    delay(3000);
    PanicWriteBanner(code, reason, file, line);
  }
#endif
}

void Panic(const char* reason) {
  PanicAt(HX_PANIC_UNKNOWN, reason, nullptr, 0);
}
