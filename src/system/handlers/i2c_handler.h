/*
  HexaOS - i2c_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  I2C domain handler for HexaOS.
  Owns device registration, transaction routing, availability policy,
  bus recovery, and runtime statistics for all I2C buses.

  Drivers must register their devices here and use this handler for all
  I2C transactions. Direct use of i2c_adapter is not permitted from
  drivers, services, or modules — the handler is the single stable
  domain boundary above the adapter.

  Device availability policy:
    After HX_I2C_DEVICE_FAILURE_THRESHOLD consecutive transaction
    failures, the device is marked unavailable. Subsequent calls
    return false immediately without touching the bus, preventing
    log spam when a sensor is disconnected. Availability is restored
    after a successful bus recovery or explicit re-enable.

  Bus recovery:
    I2cHandlerRecoverBus() sends a 9-clock-pulse reset sequence via
    the adapter to unstick a locked SDA line. On success all devices
    on that port are re-enabled and their consecutive failure counter
    is reset, giving every device a clean chance after the bus heals.

  Runtime statistics:
    Per-bus and per-device counters are updated on every transaction.
    Commands and services may query them for health monitoring and
    introspection without accessing driver or adapter internals.

  Thread safety:
    Device registration and unregistration are protected by a spinlock.
    Per-device stat counters are updated without locking — individual
    32-bit increments are atomic on all supported ESP32 variants.
    Concurrent transactions on different devices are safe; the IDF
    i2c_master driver serializes bus access internally.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Opaque device handle — obtained via I2cHandlerRegisterDevice.
// Passed to all transaction and stats functions.
typedef void* HxI2cHandle;

// Per-bus runtime statistics.
struct HxI2cBusStats {
  uint32_t tx_ok;               // Successful transactions (write or write-read).
  uint32_t tx_err;              // Failed transactions.
  uint32_t rx_ok;               // Successful read transactions.
  uint32_t rx_err;              // Failed read transactions.
  uint32_t recoveries_attempted;
  uint32_t recoveries_ok;
  uint32_t last_err_uptime_ms;  // Uptime timestamp of the most recent error.
};

// Per-device runtime information and statistics.
// Returned by I2cHandlerGetDeviceAt for introspection.
struct HxI2cDeviceInfo {
  uint8_t     port;
  uint16_t    addr;
  const char* name;             // Pointer to the name passed at registration.
  bool        available;        // False when failure threshold was exceeded.
  uint32_t    tx_ok;
  uint32_t    tx_err;
  uint32_t    rx_ok;
  uint32_t    rx_err;
  uint32_t    consecutive_failures;
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// Initialize the handler and attempt to bring up all I2C bus ports found
// in the board pinmap. Ports without mapped pins are silently skipped.
// Returns true when at least one bus port is ready.
// Must be called before any other I2cHandler function.
bool I2cHandlerInit();

// ---------------------------------------------------------------------------
// Device management
// ---------------------------------------------------------------------------

// Register a device on a bus port.
// port:    0-based I2C port index.
// addr:    7-bit device address (valid range 0x08-0x77).
// freq_hz: SCL clock speed in Hz (e.g. 100000 or 400000).
// name:    human-readable identifier used for logging and introspection.
//          Must remain valid for the lifetime of the registration (use
//          string literals or statically allocated strings).
// out:     receives the device handle on success.
//
// Returns false when the port is not ready, the address is invalid or
// already registered on that port, or the registry is full.
bool I2cHandlerRegisterDevice(uint8_t port, uint16_t addr, uint32_t freq_hz,
                              const char* name, HxI2cHandle* out);

// Deregister a device and release its registry slot.
// The handle must not be used after this call.
void I2cHandlerUnregisterDevice(HxI2cHandle handle);

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

// Write bytes to a device.
// Returns false when the device is unavailable, the handle is invalid,
// or the adapter transaction fails.
// Pass timeout_ms=0 to use the adapter default.
bool I2cHandlerWrite(HxI2cHandle handle,
                     const uint8_t* data, size_t len,
                     uint32_t timeout_ms);

// Read bytes from a device.
bool I2cHandlerRead(HxI2cHandle handle,
                    uint8_t* buf, size_t len,
                    uint32_t timeout_ms);

// Write then read without releasing the bus (repeated start).
// Standard register-read pattern: send register address, receive data.
bool I2cHandlerWriteRead(HxI2cHandle handle,
                         const uint8_t* tx, size_t tx_len,
                         uint8_t* rx,       size_t rx_len,
                         uint32_t timeout_ms);

// ---------------------------------------------------------------------------
// Bus health and recovery
// ---------------------------------------------------------------------------

// Returns true when the given port has been initialized and is ready.
bool I2cHandlerBusReady(uint8_t port);

// Send a 9-clock-pulse SDA recovery sequence on the given port.
// On success: all devices on the port are re-enabled and their
// consecutive failure counters are reset.
// Recovery attempts and outcomes are reflected in the bus statistics.
bool I2cHandlerRecoverBus(uint8_t port);

// Explicitly re-enable a device that was marked unavailable.
// Use when the caller knows the device has been reconnected or replaced.
void I2cHandlerReenableDevice(HxI2cHandle handle);

// ---------------------------------------------------------------------------
// Statistics and introspection
// ---------------------------------------------------------------------------

// Returns per-bus statistics for the given port.
bool I2cHandlerGetBusStats(uint8_t port, HxI2cBusStats* out);

// Returns the number of currently registered devices across all ports.
size_t I2cHandlerDeviceCount();

// Returns device info and statistics by registration index (0-based).
// Index is stable only within a single enumeration — do not cache indices.
bool I2cHandlerGetDeviceAt(size_t index, HxI2cDeviceInfo* out);
