/*
  HexaOS - uart_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  UART bus adapter for HexaOS. Wraps the IDF UART driver.
  Supports up to HX_UART_PORT_MAX ports. RX and TX pins are resolved from
  the board pinmap. Baud rate and mode are supplied by the caller since
  they are protocol-level, not board-level, configuration.

  RS485 half-duplex mode:
    When rs485_de_gpio >= 0 in HxUartConfig, the IDF RS485 half-duplex
    mode is enabled and IDF drives the DE pin automatically during
    transmit. This removes the need for manual GPIO toggling before
    each write and is the correct approach for Modbus RTU and similar
    protocols. The RE pin (active-low receive enable) is typically tied
    to an inverted DE signal at the hardware level.

  Buffer sizes:
    HX_UART_RX_BUF_SIZE controls the IDF ring buffer depth per port.
    For Modbus polling with many devices, a larger buffer reduces the
    risk of overrun between polls. Override in hx_build.h if needed.

  Thread safety:
    IDF UART driver calls are thread-safe. UartAdapterWrite and
    UartAdapterRead may be called from different tasks on the same port.

  UART0 note:
    On most ESP32 boards, UART0 is the IDF debug console at boot.
    Calling UartAdapterInit(0, ...) on a board that uses UART0 for
    debug output will conflict. Avoid port 0 unless the board explicitly
    routes a user peripheral to UART0 and the debug console is disabled.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum number of UART ports supported across all ESP32 family members.
// ESP32-P4 has 5; ESP32 classic and S3 have 3; C3/C6 have 2.
#define HX_UART_PORT_MAX 5

// IDF ring buffer sizes for RX and TX per port.
// For Modbus with large device populations, increase HX_UART_RX_BUF_SIZE.
#ifndef HX_UART_RX_BUF_SIZE
  #define HX_UART_RX_BUF_SIZE 1024
#endif
#ifndef HX_UART_TX_BUF_SIZE
  #define HX_UART_TX_BUF_SIZE 256
#endif

// Default write and read timeouts when the caller passes 0.
#ifndef HX_UART_DEFAULT_WRITE_TIMEOUT_MS
  #define HX_UART_DEFAULT_WRITE_TIMEOUT_MS 100
#endif
#ifndef HX_UART_DEFAULT_READ_TIMEOUT_MS
  #define HX_UART_DEFAULT_READ_TIMEOUT_MS  50
#endif

// Initialize a UART port. RX and TX pins are resolved from the pinmap.
// baud_rate:    serial baud rate (e.g. 9600, 19200, 115200).
// rs485_de_gpio: GPIO for RS485 DE control.
//   -1  = no software DE control. Use for standard full-duplex UART or
//         RS485 transceivers with automatic direction control (auto-DE).
//   >= 0 = manual DE pin. IDF RS485 half-duplex mode is enabled and IDF
//          drives this GPIO automatically during transmit via the UART
//          RTS signal. Use for transceivers such as MAX485.
// Returns false when pins are not mapped, the port does not exist on
// the active chip, or IDF driver installation fails.
bool UartAdapterInit(uint8_t port, uint32_t baud_rate, int rs485_de_gpio);

// Returns true when the port has been successfully initialized.
bool UartAdapterReady(uint8_t port);

// Returns true when RX and TX pins are mapped for the given port in the
// board pinmap and the port exists on the active chip.
// Does not initialize anything — used for port discovery at startup.
bool UartAdapterPortMapped(uint8_t port);

// Deinitialize a port and release IDF UART driver resources.
void UartAdapterDeinit(uint8_t port);

// Write bytes to the TX FIFO. Blocks until all bytes are queued or
// timeout expires. Pass timeout_ms=0 to use HX_UART_DEFAULT_WRITE_TIMEOUT_MS.
bool UartAdapterWrite(uint8_t port,
                      const uint8_t* data, size_t len,
                      uint32_t timeout_ms);

// Read up to max_len bytes from the RX ring buffer.
// Blocks up to timeout_ms waiting for at least one byte to arrive.
// Returns the number of bytes actually read (0 on timeout or error).
// Pass timeout_ms=0 to use HX_UART_DEFAULT_READ_TIMEOUT_MS.
size_t UartAdapterRead(uint8_t port,
                       uint8_t* buf, size_t max_len,
                       uint32_t timeout_ms);

// Returns the number of bytes currently waiting in the RX ring buffer.
// Non-blocking. Useful for polling-style consumers before committing
// to a blocking read.
size_t UartAdapterReadAvailable(uint8_t port);

// Discard all bytes currently in the RX ring buffer.
void UartAdapterFlushRx(uint8_t port);
