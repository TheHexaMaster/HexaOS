/*
  HexaOS - panic.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Fatal panic handler.
  Provides the last-resort stop path for unrecoverable startup or runtime failures and keeps the system halted after logging the panic reason.
*/

#include "panic.h"

#include "system/core/log.h"
#include "system/adapters/console_adapter.h"

#include <esp32-hal.h>

void Panic(const char* reason) {
  ConsoleAdapterInit();
  delay(10);

  LogError("PANIC: %s", reason ? reason : "unknown");

  while (true) {
    delay(1000);
  }
}
