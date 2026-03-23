/*
  HexaOS - uart_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  UART domain handler for HexaOS.
  Owns port lifecycle, stream statistics, and runtime introspection for
  all UART ports. Drivers and services must use this handler for all UART
  operations. Direct use of uart_adapter is not permitted from above this
  layer.

  UART vs I2C handler design difference:
    UART is a byte-stream bus, not a transaction bus. There is no device
    registry — each port is a single stream owned by one caller (typically
    a protocol driver or service). Multiple services must not share one
    UART port. The handler validates that a port is not initialized twice.

  RS485 mode:
    RS485 mode is tracked per-port for introspection and logging.
    The DE pin management is handled by the IDF UART driver automatically
    when rs485_de_gpio >= 0 in HxUartConfig (see HxUartConfig below).
    Frame-level concerns (inter-frame silence, Modbus address resolution,
    request-response pairing) are the responsibility of the protocol driver
    or service, not this handler.

  Statistics:
    Per-port byte counters and error events are updated on every operation.
    read_timeouts counts reads that returned 0 bytes — for RS485/Modbus
    this indicates a slave device did not respond within the timeout window.

  Thread safety:
    Each port is expected to be owned by a single service or driver task.
    Concurrent use of the same port from multiple tasks is not supported
    at the handler level — callers are responsible for coordination.
    Different ports may be used concurrently without restriction.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// UART port configuration supplied by the caller.
// Pins are resolved from the board pinmap internally.
struct HxUartConfig {
  uint32_t baud_rate;     // Serial baud rate (e.g. 9600, 19200, 115200).
  int      rs485_de_gpio; // GPIO for RS485 DE control.
                          // -1  = no software DE control. Use for standard
                          //       full-duplex UART or RS485 transceivers with
                          //       automatic direction control (auto-DE).
                          // >= 0 = manual DE pin. IDF drives this GPIO
                          //        automatically during transmit (MAX485 etc).
};

// Per-port runtime statistics.
struct HxUartPortStats {
  uint32_t bytes_tx;            // Total bytes successfully written.
  uint32_t bytes_rx;            // Total bytes successfully read.
  uint32_t write_errors;        // Write calls that returned false.
  uint32_t read_timeouts;       // Read calls that returned 0 bytes.
                                // For RS485/Modbus: indicates no slave response.
  uint32_t last_err_uptime_ms;  // Uptime timestamp of the most recent error.
};

// Per-port configuration and state for introspection.
struct HxUartPortInfo {
  bool     ready;      // True when the port has been successfully initialized.
  uint32_t baud_rate;  // Configured baud rate.
  bool     rs485;      // True when RS485 half-duplex mode is active (manual DE).
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// Scan all UART ports for mapped pins and log their availability.
// Does not initialize any port — initialization is deferred to the owning
// driver or service via UartHandlerInit().
// Returns the number of ports that have pins mapped in the board pinmap.
uint8_t UartHandlerDiscoverPorts();

// Initialize a UART port. Wraps UartAdapterInit and records port state.
// Returns false when the port is already initialized, pins are not mapped,
// the port does not exist on the active chip, or IDF init fails.
bool UartHandlerInit(uint8_t port, const HxUartConfig* cfg);

// Returns true when the port has been successfully initialized.
bool UartHandlerReady(uint8_t port);

// Deinitialize a port and release all resources.
void UartHandlerDeinit(uint8_t port);

// ---------------------------------------------------------------------------
// Stream operations
// ---------------------------------------------------------------------------

// Write bytes to the TX FIFO. Updates bytes_tx or write_errors.
// Pass timeout_ms=0 to use the adapter default.
bool UartHandlerWrite(uint8_t port,
                      const uint8_t* data, size_t len,
                      uint32_t timeout_ms);

// Read up to max_len bytes from the RX ring buffer.
// Blocks up to timeout_ms. Returns bytes read; 0 on timeout or error.
// Updates bytes_rx or read_timeouts.
size_t UartHandlerRead(uint8_t port,
                       uint8_t* buf, size_t max_len,
                       uint32_t timeout_ms);

// Returns the number of bytes waiting in the RX ring buffer.
// Non-blocking. Use before UartHandlerRead to avoid unnecessary blocking.
size_t UartHandlerReadAvailable(uint8_t port);

// Discard all bytes currently in the RX ring buffer.
void UartHandlerFlushRx(uint8_t port);

// ---------------------------------------------------------------------------
// Introspection
// ---------------------------------------------------------------------------

// Returns per-port statistics.
bool UartHandlerGetStats(uint8_t port, HxUartPortStats* out);

// Returns port configuration and ready state.
bool UartHandlerGetPortInfo(uint8_t port, HxUartPortInfo* out);
