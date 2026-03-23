/*
  HexaOS - spi_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  SPI domain handler for HexaOS.
  Owns device registration, transaction routing, availability policy,
  and runtime statistics for all SPI buses.

  Drivers must register their devices here and use this handler for all
  SPI transactions. Direct use of spi_adapter is not permitted from
  drivers, services, or modules — the handler is the single stable
  domain boundary above the adapter.

  Device availability policy:
    After HX_SPI_DEVICE_FAILURE_THRESHOLD consecutive transaction
    failures the device is marked unavailable. Subsequent calls return
    false immediately without touching the bus. Availability is restored
    after a successful transaction following explicit re-enable via
    SpiHandlerReenableDevice().

  Bus recovery:
    SPI does not have a bus lock-up mechanism equivalent to I2C SDA
    hold. No recovery function is provided. If a device becomes
    permanently unresponsive, the caller should unregister and
    re-register it.

  Runtime statistics:
    Per-bus and per-device counters including bytes_transferred are
    updated on every transaction. bytes_transferred is relevant for
    high-throughput SPI devices such as displays and flash chips.

  Thread safety:
    Device registration and unregistration are protected by a critical
    section. Per-device stat counters are updated without locking —
    individual 32-bit increments are atomic on all supported ESP32
    variants. Concurrent transactions on different devices are safe;
    the IDF spi_master driver serializes bus access internally.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Opaque device handle — obtained via SpiHandlerRegisterDevice.
typedef void* HxSpiHandle;

// Per-bus runtime statistics.
struct HxSpiBusStats {
  uint32_t tx_ok;               // Successful transactions.
  uint32_t tx_err;              // Failed transactions.
  uint32_t bytes_transferred;   // Total bytes moved across the bus.
  uint32_t last_err_uptime_ms;
};

// Per-device runtime information and statistics.
struct HxSpiDeviceInfo {
  uint8_t     port;
  int         cs_pin;
  const char* name;
  bool        available;
  uint32_t    tx_ok;
  uint32_t    tx_err;
  uint32_t    bytes_transferred;
  uint32_t    consecutive_failures;
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// Initialize the handler and attempt to bring up all SPI bus ports found
// in the board pinmap. Ports without mapped pins are silently skipped.
// Returns true when at least one bus port is ready.
bool SpiHandlerInit();

// Returns true when the given port has been successfully initialized.
bool SpiHandlerBusReady(uint8_t port);

// ---------------------------------------------------------------------------
// Device management
// ---------------------------------------------------------------------------

// Register a device on a bus port.
// port:    0-based SPI port index.
// cs_pin:  GPIO number for chip select. Pass -1 to manage CS manually.
// freq_hz: SPI clock speed in Hz.
// mode:    SPI mode 0-3 (CPOL/CPHA encoding).
// name:    human-readable identifier for logging and introspection.
//          Must remain valid for the lifetime of the registration.
//
// Returns false when the port is not ready, the CS pin is already in use
// on that port, the registry is full, or the adapter call fails.
bool SpiHandlerRegisterDevice(uint8_t port, int cs_pin, uint32_t freq_hz,
                              uint8_t mode, const char* name,
                              HxSpiHandle* out);

// Deregister a device and release its registry slot.
// The handle must not be used after this call.
void SpiHandlerUnregisterDevice(HxSpiHandle handle);

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

// Full-duplex transfer.
// tx may be NULL for read-only; rx may be NULL for write-only.
// len is the transfer length in bytes.
// Pass timeout_ms=0 to use the adapter default.
bool SpiHandlerTransfer(HxSpiHandle handle,
                        const uint8_t* tx, uint8_t* rx, size_t len,
                        uint32_t timeout_ms);

// Convenience transmit-only.
bool SpiHandlerTransmit(HxSpiHandle handle,
                        const uint8_t* tx, size_t len,
                        uint32_t timeout_ms);

// ---------------------------------------------------------------------------
// Availability
// ---------------------------------------------------------------------------

// Explicitly re-enable a device that was marked unavailable.
void SpiHandlerReenableDevice(HxSpiHandle handle);

// ---------------------------------------------------------------------------
// Statistics and introspection
// ---------------------------------------------------------------------------

// Returns per-bus statistics for the given port.
bool SpiHandlerGetBusStats(uint8_t port, HxSpiBusStats* out);

// Returns the number of currently registered devices across all ports.
size_t SpiHandlerDeviceCount();

// Returns device info and statistics by registration index (0-based).
bool SpiHandlerGetDeviceAt(size_t index, HxSpiDeviceInfo* out);
