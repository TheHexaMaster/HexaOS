/*
  HexaOS - spi_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  SPI bus adapter for HexaOS. Wraps the IDF spi_master driver.
  Supports up to HX_SPI_PORT_MAX bus ports. Each port is initialized once
  from the board pinmap. Devices are registered per-port with their own
  CS pin, clock speed, and mode. All bus operations go through an opaque
  device handle.

  Initialization:
    Call SpiAdapterBusInit(port) for each bus. Ports with no MOSI/SCLK
    in the pinmap return false and remain inactive.
    MISO is optional — set to -1 for write-only buses (e.g. display-only).

  Device lifetime:
    SpiAdapterAddDevice() allocates a device slot on the bus. The returned
    handle is valid until SpiAdapterRemoveDevice() is called. Multiple
    devices may share one bus with different CS pins.

  Thread safety:
    The IDF spi_master driver serializes transactions per device internally.
    Concurrent transfers on the same bus from different tasks are safe.

  Transfer size:
    HX_SPI_MAX_TRANSFER_SIZE controls the DMA-capable transfer ceiling.
    Override in hx_build.h when display frame buffers or large payloads
    are expected.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum number of user-accessible SPI bus ports.
// ESP32 / S2 / S3 / P4 have 2 user ports (SPI2, SPI3).
// ESP32-C3 / C6 have 1 (SPI2 only).
#define HX_SPI_PORT_MAX 2

// DMA transfer size ceiling in bytes. Covers most display frame buffers.
#ifndef HX_SPI_MAX_TRANSFER_SIZE
  #define HX_SPI_MAX_TRANSFER_SIZE 8192
#endif

// Default transaction timeout when the caller passes 0.
#ifndef HX_SPI_DEFAULT_TIMEOUT_MS
  #define HX_SPI_DEFAULT_TIMEOUT_MS 100
#endif

// Opaque device handle — obtained via SpiAdapterAddDevice.
// Internally maps to spi_device_handle_t.
typedef void* HxSpiDevHandle;

// Initialize a bus port from the board pinmap.
// port: 0-based index (0 = SPI2_HOST, 1 = SPI3_HOST where available).
// MOSI and SCLK are mandatory. MISO is optional (-1 accepted for TX-only buses).
// Returns false when mandatory pins are not mapped, when the port does not
// exist on the active chip, or when IDF init fails.
bool SpiAdapterBusInit(uint8_t port);

// Returns true when the port has been successfully initialized.
bool SpiAdapterBusReady(uint8_t port);

// Register a device on an initialized bus.
// cs_pin:  GPIO number for chip select. Pass -1 to manage CS manually.
// freq_hz: SPI clock speed in Hz.
// mode:    SPI mode 0-3 (CPOL/CPHA encoding).
// out:     receives the device handle on success.
bool SpiAdapterAddDevice(uint8_t port, int cs_pin, uint32_t freq_hz,
                         uint8_t mode, HxSpiDevHandle* out);

// Deregister a device. The handle must not be used after this call.
void SpiAdapterRemoveDevice(HxSpiDevHandle handle);

// Full-duplex transfer.
// tx may be NULL for read-only operations.
// rx may be NULL for write-only operations.
// len is the transfer length in bytes (tx and rx must both accommodate len).
// Pass timeout_ms=0 to use HX_SPI_DEFAULT_TIMEOUT_MS.
bool SpiAdapterTransfer(HxSpiDevHandle handle,
                        const uint8_t* tx, uint8_t* rx, size_t len,
                        uint32_t timeout_ms);

// Convenience transmit-only (DMA-capable for large payloads).
bool SpiAdapterTransmit(HxSpiDevHandle handle,
                        const uint8_t* tx, size_t len,
                        uint32_t timeout_ms);
