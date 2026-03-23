/*
  HexaOS - uart_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  UART domain handler implementation.
  All UART stream operations from drivers and services go through this
  handler. The adapter is never called directly from above this layer.
*/

#include "uart_handler.h"

#include "headers/hx_build.h"
#include "system/adapters/uart_adapter.h"
#include "system/core/log.h"
#include "system/core/time.h"

static constexpr const char* TAG = "UART";

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

struct HxUartPortState {
  bool           ready;
  uint32_t       baud_rate;
  bool           rs485;
  HxUartPortStats stats;
};

static HxUartPortState g_ports[HX_UART_PORT_MAX] = {};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

uint8_t UartHandlerDiscoverPorts() {
  uint8_t count = 0;
  for (uint8_t p = 0; p < HX_UART_PORT_MAX; p++) {
    if (UartAdapterPortMapped(p)) {
      HX_LOGI(TAG, "UART%u available", (unsigned)p);
      count++;
    }
  }
  HX_LOGI(TAG, "%u UART port(s) available", (unsigned)count);
  return count;
}

bool UartHandlerInit(uint8_t port, const HxUartConfig* cfg) {
  if (!cfg || port >= HX_UART_PORT_MAX) { return false; }

  if (g_ports[port].ready) {
    HX_LOGW(TAG, "UART%u already initialized — ignoring duplicate init", (unsigned)port);
    return false;
  }

  if (!UartAdapterInit(port, cfg->baud_rate, cfg->rs485_de_gpio)) { return false; }

  g_ports[port].ready     = true;
  g_ports[port].baud_rate = cfg->baud_rate;
  g_ports[port].rs485     = (cfg->rs485_de_gpio >= 0);
  g_ports[port].stats     = {};

  HX_LOGI(TAG, "UART%u handler ready baud=%lu %s",
          (unsigned)port,
          (unsigned long)cfg->baud_rate,
          g_ports[port].rs485 ? "RS485-half-duplex" : "full-duplex");
  return true;
}

bool UartHandlerReady(uint8_t port) {
  return (port < HX_UART_PORT_MAX) && g_ports[port].ready;
}

void UartHandlerDeinit(uint8_t port) {
  if (port >= HX_UART_PORT_MAX || !g_ports[port].ready) { return; }
  UartAdapterDeinit(port);
  g_ports[port] = {};
  HX_LOGI(TAG, "UART%u deinitialized", (unsigned)port);
}

// ---------------------------------------------------------------------------
// Stream operations
// ---------------------------------------------------------------------------

bool UartHandlerWrite(uint8_t port,
                      const uint8_t* data, size_t len,
                      uint32_t timeout_ms) {
  if (port >= HX_UART_PORT_MAX || !g_ports[port].ready) { return false; }

  bool ok = UartAdapterWrite(port, data, len, timeout_ms);
  if (ok) {
    g_ports[port].stats.bytes_tx += (uint32_t)len;
  } else {
    g_ports[port].stats.write_errors++;
    g_ports[port].stats.last_err_uptime_ms = TimeMonotonicMs32();
    HX_LOGW(TAG, "UART%u write error (len=%u)", (unsigned)port, (unsigned)len);
  }
  return ok;
}

size_t UartHandlerRead(uint8_t port,
                       uint8_t* buf, size_t max_len,
                       uint32_t timeout_ms) {
  if (port >= HX_UART_PORT_MAX || !g_ports[port].ready) { return 0; }

  size_t n = UartAdapterRead(port, buf, max_len, timeout_ms);
  if (n > 0) {
    g_ports[port].stats.bytes_rx += (uint32_t)n;
  } else {
    // Zero bytes returned = timeout or empty buffer.
    // For RS485/Modbus callers this typically means a slave did not respond.
    g_ports[port].stats.read_timeouts++;
  }
  return n;
}

size_t UartHandlerReadAvailable(uint8_t port) {
  if (port >= HX_UART_PORT_MAX || !g_ports[port].ready) { return 0; }
  return UartAdapterReadAvailable(port);
}

void UartHandlerFlushRx(uint8_t port) {
  if (port >= HX_UART_PORT_MAX || !g_ports[port].ready) { return; }
  UartAdapterFlushRx(port);
}

// ---------------------------------------------------------------------------
// Introspection
// ---------------------------------------------------------------------------

bool UartHandlerGetStats(uint8_t port, HxUartPortStats* out) {
  if (!out || port >= HX_UART_PORT_MAX) { return false; }
  *out = g_ports[port].stats;
  return true;
}

bool UartHandlerGetPortInfo(uint8_t port, HxUartPortInfo* out) {
  if (!out || port >= HX_UART_PORT_MAX) { return false; }
  out->ready     = g_ports[port].ready;
  out->baud_rate = g_ports[port].baud_rate;
  out->rs485     = g_ports[port].rs485;
  return true;
}
