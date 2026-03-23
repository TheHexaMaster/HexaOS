/*
  HexaOS - i2c_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  I2C bus adapter for HexaOS. Wraps the IDF i2c_master driver (IDF 5.1+).
  Supports up to HX_I2C_PORT_MAX bus ports. Each port is initialized once
  from the board pinmap. Devices are registered per-port and identified by
  an opaque handle used for all subsequent transfers.

  Initialization:
    Call I2cAdapterBusInit(port) for each bus. Ports with no pins in the
    pinmap return false and remain inactive — not an error on boards that
    do not wire that bus.

  Device lifetime:
    I2cAdapterAddDevice() registers a device on an initialized bus and
    returns an opaque handle. The handle remains valid until
    I2cAdapterRemoveDevice() is called. Drivers must not use a handle
    after removal.

  Thread safety:
    The IDF i2c_master driver serializes bus transactions internally.
    Multiple callers may use handles from the same bus concurrently.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum number of I2C bus ports supported across all ESP32 family members.
// ESP32-P4 has 3; most other variants have 1 or 2.
#define HX_I2C_PORT_MAX 3

// Default transaction timeout when the caller passes 0.
// Covers typical sensor clock-stretching scenarios.
#ifndef HX_I2C_DEFAULT_TIMEOUT_MS
  #define HX_I2C_DEFAULT_TIMEOUT_MS 50
#endif

// Timeout used for address probing during bus scans.
// Shorter than the transaction default — devices either ACK immediately
// or not at all.
#ifndef HX_I2C_PROBE_TIMEOUT_MS
  #define HX_I2C_PROBE_TIMEOUT_MS 10
#endif

// Opaque device handle — obtained via I2cAdapterAddDevice.
// Internally maps to i2c_master_dev_handle_t.
typedef void* HxI2cDevHandle;

// Initialize a bus port from the board pinmap.
// port: 0-based index (0 = I2C0, 1 = I2C1, 2 = I2C2).
// Returns false when SCL or SDA are not mapped for this port, when
// the port does not exist on the active chip, or when IDF init fails.
bool I2cAdapterBusInit(uint8_t port);

// Returns true when the port has been successfully initialized.
bool I2cAdapterBusReady(uint8_t port);

// Register a device on an initialized bus.
// addr:    7-bit device address.
// freq_hz: SCL clock speed in Hz (e.g. 100000 or 400000).
// out:     receives the device handle on success.
bool I2cAdapterAddDevice(uint8_t port, uint16_t addr, uint32_t freq_hz,
                         HxI2cDevHandle* out);

// Deregister a device. The handle must not be used after this call.
void I2cAdapterRemoveDevice(HxI2cDevHandle handle);

// Send a 9-clock-pulse reset sequence to unstick a locked SDA line.
// Returns true when the IDF bus reset completes without error.
// Recovery policy (when to call this, what to do after) belongs to the handler.
bool I2cAdapterRecoverBus(uint8_t port);

// Write bytes to a device. Pass timeout_ms=0 to use HX_I2C_DEFAULT_TIMEOUT_MS.
bool I2cAdapterWrite(HxI2cDevHandle handle,
                     const uint8_t* data, size_t len,
                     uint32_t timeout_ms);

// Read bytes from a device.
bool I2cAdapterRead(HxI2cDevHandle handle,
                    uint8_t* buf, size_t len,
                    uint32_t timeout_ms);

// Write then read without releasing the bus (repeated start).
// Typical register-read pattern: send register address, then receive data.
bool I2cAdapterWriteRead(HxI2cDevHandle handle,
                         const uint8_t* tx, size_t tx_len,
                         uint8_t* rx,       size_t rx_len,
                         uint32_t timeout_ms);

// Probe a single address on an initialized bus without registering a device.
// Returns true if a device acknowledges at the given address.
// Pass timeout_ms=0 to use HX_I2C_PROBE_TIMEOUT_MS.
bool I2cAdapterProbe(uint8_t port, uint16_t addr, uint32_t timeout_ms);
