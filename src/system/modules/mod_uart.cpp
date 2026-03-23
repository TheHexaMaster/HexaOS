/*
  HexaOS - mod_uart.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  UART lifecycle module for HexaOS.
  At startup, delegates port discovery to the UART handler, which scans
  the board pinmap and logs which ports have pins mapped. Individual ports
  are not initialized here — initialization is deferred to the owning
  driver or service when it starts.

  Future revisions will add port ownership tracking and runtime driver
  binding management via the handler.
*/

#include "system/core/module_registry.h"
#include "system/core/log.h"
#include "system/handlers/uart_handler.h"

static constexpr const char* TAG = "UART";

static bool UartInit() {
  if (UartHandlerDiscoverPorts() == 0) {
    HX_LOGW(TAG, "no UART ports available");
    return false;
  }
  return true;
}

const HxModule ModuleUart = {
  .name        = "uart",
  .init        = UartInit,
  .start       = nullptr,
  .loop        = nullptr,
  .every_10ms  = nullptr,
  .every_100ms = nullptr,
  .every_1s    = nullptr
};
