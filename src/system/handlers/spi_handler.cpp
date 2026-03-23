/*
  HexaOS - spi_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  SPI domain handler implementation.
  All SPI transactions from drivers, services, and modules go through
  this handler. The adapter is never called directly from above this layer.
*/

#include "spi_handler.h"

#include "headers/hx_build.h"
#include "system/adapters/spi_adapter.h"
#include "system/core/log.h"
#include "system/core/rtos.h"
#include "system/core/time.h"

static constexpr const char* TAG = "SPI";

// ---------------------------------------------------------------------------
// Device registry entry
// ---------------------------------------------------------------------------

struct HxSpiDevice {
  bool           active;
  uint8_t        port;
  int            cs_pin;
  const char*    name;
  HxSpiDevHandle adapter_handle;

  bool           available;
  uint32_t       consecutive_failures;

  uint32_t       tx_ok;
  uint32_t       tx_err;
  uint32_t       bytes_transferred;
};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static HxSpiDevice    g_devices[HX_SPI_DEVICE_MAX] = {};
static HxSpiBusStats  g_bus_stats[HX_SPI_PORT_MAX]  = {};
static HxRtosCritical g_registry_lock               = HX_RTOS_CRITICAL_INIT;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static HxSpiDevice* device_from_handle(HxSpiHandle handle) {
  if (!handle) { return nullptr; }
  HxSpiDevice* dev = (HxSpiDevice*)handle;
  if (dev < &g_devices[0] || dev >= &g_devices[HX_SPI_DEVICE_MAX]) { return nullptr; }
  if (!dev->active) { return nullptr; }
  return dev;
}

static void on_transfer_ok(HxSpiDevice* dev, size_t bytes) {
  dev->tx_ok++;
  dev->bytes_transferred   += (uint32_t)bytes;
  dev->consecutive_failures = 0;
  g_bus_stats[dev->port].tx_ok++;
  g_bus_stats[dev->port].bytes_transferred += (uint32_t)bytes;
}

static void on_transfer_err(HxSpiDevice* dev) {
  dev->tx_err++;
  dev->consecutive_failures++;
  g_bus_stats[dev->port].tx_err++;
  g_bus_stats[dev->port].last_err_uptime_ms = TimeMonotonicMs32();

#if HX_SPI_DEVICE_FAILURE_THRESHOLD > 0
  if (dev->available &&
      dev->consecutive_failures >= (uint32_t)HX_SPI_DEVICE_FAILURE_THRESHOLD) {
    dev->available = false;
    HX_LOGW(TAG, "SPI%u cs=%d [%s] marked unavailable after %lu failures",
            (unsigned)dev->port, dev->cs_pin, dev->name,
            (unsigned long)dev->consecutive_failures);
  }
#endif
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool SpiHandlerInit() {
  RtosCriticalInit(&g_registry_lock);
  bool any = false;
  for (uint8_t p = 0; p < HX_SPI_PORT_MAX; p++) {
    if (SpiAdapterBusInit(p)) { any = true; }
  }
  return any;
}

bool SpiHandlerBusReady(uint8_t port) {
  return SpiAdapterBusReady(port);
}

// ---------------------------------------------------------------------------
// Device management
// ---------------------------------------------------------------------------

bool SpiHandlerRegisterDevice(uint8_t port, int cs_pin, uint32_t freq_hz,
                              uint8_t mode, const char* name,
                              HxSpiHandle* out) {
  if (!out || !name) { return false; }

  if (!SpiAdapterBusReady(port)) {
    HX_LOGE(TAG, "register [%s]: SPI%u not ready", name, (unsigned)port);
    return false;
  }

  RtosCriticalEnter(&g_registry_lock);

  // Check for duplicate CS pin on the same port.
  for (int i = 0; i < HX_SPI_DEVICE_MAX; i++) {
    if (g_devices[i].active &&
        g_devices[i].port   == port &&
        g_devices[i].cs_pin == cs_pin) {
      RtosCriticalExit(&g_registry_lock);
      HX_LOGE(TAG, "register [%s]: SPI%u cs=%d already registered",
              name, (unsigned)port, cs_pin);
      return false;
    }
  }

  int slot = -1;
  for (int i = 0; i < HX_SPI_DEVICE_MAX; i++) {
    if (!g_devices[i].active) { slot = i; break; }
  }

  RtosCriticalExit(&g_registry_lock);

  if (slot < 0) {
    HX_LOGE(TAG, "register [%s]: device registry full (max=%d)",
            name, HX_SPI_DEVICE_MAX);
    return false;
  }

  HxSpiDevHandle adapter_handle = nullptr;
  if (!SpiAdapterAddDevice(port, cs_pin, freq_hz, mode, &adapter_handle)) {
    return false;
  }

  RtosCriticalEnter(&g_registry_lock);
  HxSpiDevice* dev           = &g_devices[slot];
  dev->active                = true;
  dev->port                  = port;
  dev->cs_pin                = cs_pin;
  dev->name                  = name;
  dev->adapter_handle        = adapter_handle;
  dev->available             = true;
  dev->consecutive_failures  = 0;
  dev->tx_ok = dev->tx_err = dev->bytes_transferred = 0;
  RtosCriticalExit(&g_registry_lock);

  *out = (HxSpiHandle)dev;
  HX_LOGI(TAG, "SPI%u cs=%d [%s] registered", (unsigned)port, cs_pin, name);
  return true;
}

void SpiHandlerUnregisterDevice(HxSpiHandle handle) {
  HxSpiDevice* dev = device_from_handle(handle);
  if (!dev) { return; }

  SpiAdapterRemoveDevice(dev->adapter_handle);

  RtosCriticalEnter(&g_registry_lock);
  HX_LOGI(TAG, "SPI%u cs=%d [%s] unregistered",
          (unsigned)dev->port, dev->cs_pin, dev->name);
  *dev = {};
  RtosCriticalExit(&g_registry_lock);
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

bool SpiHandlerTransfer(HxSpiHandle handle,
                        const uint8_t* tx, uint8_t* rx, size_t len,
                        uint32_t timeout_ms) {
  HxSpiDevice* dev = device_from_handle(handle);
  if (!dev) { return false; }
  if (!dev->available) {
    HX_LOGLL(TAG, "SPI%u cs=%d [%s] transfer skipped — unavailable",
             (unsigned)dev->port, dev->cs_pin, dev->name);
    return false;
  }

  bool ok = SpiAdapterTransfer(dev->adapter_handle, tx, rx, len, timeout_ms);
  ok ? on_transfer_ok(dev, len) : on_transfer_err(dev);
  return ok;
}

bool SpiHandlerTransmit(HxSpiHandle handle,
                        const uint8_t* tx, size_t len,
                        uint32_t timeout_ms) {
  return SpiHandlerTransfer(handle, tx, nullptr, len, timeout_ms);
}

// ---------------------------------------------------------------------------
// Availability
// ---------------------------------------------------------------------------

void SpiHandlerReenableDevice(HxSpiHandle handle) {
  HxSpiDevice* dev = device_from_handle(handle);
  if (!dev) { return; }
  RtosCriticalEnter(&g_registry_lock);
  dev->available            = true;
  dev->consecutive_failures = 0;
  RtosCriticalExit(&g_registry_lock);
  HX_LOGI(TAG, "SPI%u cs=%d [%s] re-enabled",
          (unsigned)dev->port, dev->cs_pin, dev->name);
}

// ---------------------------------------------------------------------------
// Statistics and introspection
// ---------------------------------------------------------------------------

bool SpiHandlerGetBusStats(uint8_t port, HxSpiBusStats* out) {
  if (!out || port >= HX_SPI_PORT_MAX) { return false; }
  *out = g_bus_stats[port];
  return true;
}

size_t SpiHandlerDeviceCount() {
  size_t count = 0;
  for (int i = 0; i < HX_SPI_DEVICE_MAX; i++) {
    if (g_devices[i].active) { count++; }
  }
  return count;
}

bool SpiHandlerGetDeviceAt(size_t index, HxSpiDeviceInfo* out) {
  if (!out) { return false; }
  size_t seen = 0;
  for (int i = 0; i < HX_SPI_DEVICE_MAX; i++) {
    if (!g_devices[i].active) { continue; }
    if (seen == index) {
      out->port                 = g_devices[i].port;
      out->cs_pin               = g_devices[i].cs_pin;
      out->name                 = g_devices[i].name;
      out->available            = g_devices[i].available;
      out->tx_ok                = g_devices[i].tx_ok;
      out->tx_err               = g_devices[i].tx_err;
      out->bytes_transferred    = g_devices[i].bytes_transferred;
      out->consecutive_failures = g_devices[i].consecutive_failures;
      return true;
    }
    seen++;
  }
  return false;
}
