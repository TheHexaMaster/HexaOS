/*
  HexaOS - i2c_handler.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  I2C domain handler implementation.
  All I2C transactions from drivers, services, and modules go through this
  handler. The adapter is never called directly from above this layer.
*/

#include "i2c_handler.h"

#include "headers/hx_build.h"
#include "system/adapters/i2c_adapter.h"
#include "system/core/log.h"
#include "system/core/rtos.h"
#include "system/core/time.h"

static constexpr const char* TAG = "I2C";

// ---------------------------------------------------------------------------
// I2C reserved address range validation
// 0x00-0x07 and 0x78-0x7F are reserved per the I2C specification.
// ---------------------------------------------------------------------------

static inline bool addr_valid(uint16_t addr) {
  return (addr >= 0x08) && (addr <= 0x77);
}

// ---------------------------------------------------------------------------
// Device registry entry
// ---------------------------------------------------------------------------

struct HxI2cDevice {
  bool           active;
  uint8_t        port;
  uint16_t       addr;
  const char*    name;
  HxI2cDevHandle adapter_handle;

  // Availability policy
  bool           available;
  uint32_t       consecutive_failures;

  // Per-device statistics
  uint32_t       tx_ok;
  uint32_t       tx_err;
  uint32_t       rx_ok;
  uint32_t       rx_err;
};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static HxI2cDevice     g_devices[HX_I2C_DEVICE_MAX]  = {};
static HxI2cBusStats   g_bus_stats[HX_I2C_PORT_MAX]  = {};
static HxRtosCritical  g_registry_lock                = HX_RTOS_CRITICAL_INIT;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static HxI2cDevice* device_from_handle(HxI2cHandle handle) {
  if (!handle) { return nullptr; }
  HxI2cDevice* dev = (HxI2cDevice*)handle;
  // Verify the pointer is within our registry bounds.
  if (dev < &g_devices[0] || dev >= &g_devices[HX_I2C_DEVICE_MAX]) { return nullptr; }
  if (!dev->active) { return nullptr; }
  return dev;
}

static void on_tx_ok(HxI2cDevice* dev) {
  dev->tx_ok++;
  dev->consecutive_failures = 0;
  g_bus_stats[dev->port].tx_ok++;
}

static void on_tx_err(HxI2cDevice* dev) {
  dev->tx_err++;
  dev->consecutive_failures++;
  g_bus_stats[dev->port].tx_err++;
  g_bus_stats[dev->port].last_err_uptime_ms = TimeMonotonicMs32();

#if HX_I2C_DEVICE_FAILURE_THRESHOLD > 0
  if (dev->available &&
      dev->consecutive_failures >= (uint32_t)HX_I2C_DEVICE_FAILURE_THRESHOLD) {
    dev->available = false;
    HX_LOGW(TAG, "BUS%u 0x%02x [%s] marked unavailable after %lu failures",
            (unsigned)(dev->port + 1), (unsigned)dev->addr, dev->name,
            (unsigned long)dev->consecutive_failures);
  }
#endif
}

static void on_rx_ok(HxI2cDevice* dev) {
  dev->rx_ok++;
  dev->consecutive_failures = 0;
  g_bus_stats[dev->port].rx_ok++;
}

static void on_rx_err(HxI2cDevice* dev) {
  dev->rx_err++;
  dev->consecutive_failures++;
  g_bus_stats[dev->port].rx_err++;
  g_bus_stats[dev->port].last_err_uptime_ms = TimeMonotonicMs32();

#if HX_I2C_DEVICE_FAILURE_THRESHOLD > 0
  if (dev->available &&
      dev->consecutive_failures >= (uint32_t)HX_I2C_DEVICE_FAILURE_THRESHOLD) {
    dev->available = false;
    HX_LOGW(TAG, "BUS%u 0x%02x [%s] marked unavailable after %lu failures",
            (unsigned)(dev->port + 1), (unsigned)dev->addr, dev->name,
            (unsigned long)dev->consecutive_failures);
  }
#endif
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void scan_bus(uint8_t port) {
  uint8_t found = 0;
  for (uint16_t addr = 0x08; addr <= 0x77; addr++) {
    if (I2cAdapterProbe(port, (uint16_t)addr, 0)) {
      HX_LOGI(TAG, "BUS%u 0x%02x detected", (unsigned)(port + 1), (unsigned)addr);
      found++;
    }
  }
  HX_LOGI(TAG, "BUS%u scan: %u device(s)", (unsigned)(port + 1), (unsigned)found);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool I2cHandlerInit() {
  RtosCriticalInit(&g_registry_lock);
  bool any = false;
  for (uint8_t p = 0; p < HX_I2C_PORT_MAX; p++) {
    if (I2cAdapterBusInit(p)) {
      any = true;
      scan_bus(p);
    }
  }
  return any;
}

// ---------------------------------------------------------------------------
// Device management
// ---------------------------------------------------------------------------

bool I2cHandlerRegisterDevice(uint8_t port, uint16_t addr, uint32_t freq_hz,
                              const char* name, HxI2cHandle* out) {
  if (!out || !name) { return false; }

  if (!I2cAdapterBusReady(port)) {
    HX_LOGE(TAG, "register [%s]: BUS%u not ready", name, (unsigned)(port + 1));
    return false;
  }
  if (!addr_valid(addr)) {
    HX_LOGE(TAG, "register [%s]: address 0x%02x out of valid range", name, (unsigned)addr);
    return false;
  }

  RtosCriticalEnter(&g_registry_lock);

  // Check for duplicate address on the same port.
  for (int i = 0; i < HX_I2C_DEVICE_MAX; i++) {
    if (g_devices[i].active &&
        g_devices[i].port == port &&
        g_devices[i].addr == addr) {
      RtosCriticalExit(&g_registry_lock);
      HX_LOGE(TAG, "register [%s]: BUS%u 0x%02x already registered",
              name, (unsigned)(port + 1), (unsigned)addr);
      return false;
    }
  }

  // Find a free slot.
  int slot = -1;
  for (int i = 0; i < HX_I2C_DEVICE_MAX; i++) {
    if (!g_devices[i].active) { slot = i; break; }
  }

  RtosCriticalExit(&g_registry_lock);

  if (slot < 0) {
    HX_LOGE(TAG, "register [%s]: device registry full (max=%d)",
            name, HX_I2C_DEVICE_MAX);
    return false;
  }

  HxI2cDevHandle adapter_handle = nullptr;
  if (!I2cAdapterAddDevice(port, addr, freq_hz, &adapter_handle)) {
    return false;
  }

  RtosCriticalEnter(&g_registry_lock);
  HxI2cDevice* dev       = &g_devices[slot];
  dev->active            = true;
  dev->port              = port;
  dev->addr              = addr;
  dev->name              = name;
  dev->adapter_handle    = adapter_handle;
  dev->available         = true;
  dev->consecutive_failures = 0;
  dev->tx_ok = dev->tx_err = dev->rx_ok = dev->rx_err = 0;
  RtosCriticalExit(&g_registry_lock);

  *out = (HxI2cHandle)dev;
  HX_LOGI(TAG, "BUS%u 0x%02x [%s] registered", (unsigned)(port + 1), (unsigned)addr, name);
  return true;
}

void I2cHandlerUnregisterDevice(HxI2cHandle handle) {
  HxI2cDevice* dev = device_from_handle(handle);
  if (!dev) { return; }

  I2cAdapterRemoveDevice(dev->adapter_handle);

  RtosCriticalEnter(&g_registry_lock);
  HX_LOGI(TAG, "BUS%u 0x%02x [%s] unregistered",
          (unsigned)(dev->port + 1), (unsigned)dev->addr, dev->name);
  *dev = {};  // Zero the slot and clear active flag.
  RtosCriticalExit(&g_registry_lock);
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

bool I2cHandlerWrite(HxI2cHandle handle,
                     const uint8_t* data, size_t len,
                     uint32_t timeout_ms) {
  HxI2cDevice* dev = device_from_handle(handle);
  if (!dev) { return false; }
  if (!dev->available) {
    HX_LOGLL(TAG, "BUS%u 0x%02x [%s] write skipped — unavailable",
             (unsigned)(dev->port + 1), (unsigned)dev->addr, dev->name);
    return false;
  }

  bool ok = I2cAdapterWrite(dev->adapter_handle, data, len, timeout_ms);
  ok ? on_tx_ok(dev) : on_tx_err(dev);
  return ok;
}

bool I2cHandlerRead(HxI2cHandle handle,
                    uint8_t* buf, size_t len,
                    uint32_t timeout_ms) {
  HxI2cDevice* dev = device_from_handle(handle);
  if (!dev) { return false; }
  if (!dev->available) {
    HX_LOGLL(TAG, "BUS%u 0x%02x [%s] read skipped — unavailable",
             (unsigned)(dev->port + 1), (unsigned)dev->addr, dev->name);
    return false;
  }

  bool ok = I2cAdapterRead(dev->adapter_handle, buf, len, timeout_ms);
  ok ? on_rx_ok(dev) : on_rx_err(dev);
  return ok;
}

bool I2cHandlerWriteRead(HxI2cHandle handle,
                         const uint8_t* tx, size_t tx_len,
                         uint8_t* rx,       size_t rx_len,
                         uint32_t timeout_ms) {
  HxI2cDevice* dev = device_from_handle(handle);
  if (!dev) { return false; }
  if (!dev->available) {
    HX_LOGLL(TAG, "BUS%u 0x%02x [%s] write-read skipped — unavailable",
             (unsigned)(dev->port + 1), (unsigned)dev->addr, dev->name);
    return false;
  }

  bool ok = I2cAdapterWriteRead(dev->adapter_handle,
                                 tx, tx_len, rx, rx_len, timeout_ms);
  // WriteRead counts as both a TX and RX transaction.
  ok ? (on_tx_ok(dev), on_rx_ok(dev)) : (on_tx_err(dev), on_rx_err(dev));
  return ok;
}

// ---------------------------------------------------------------------------
// Bus health and recovery
// ---------------------------------------------------------------------------

bool I2cHandlerBusReady(uint8_t port) {
  return I2cAdapterBusReady(port);
}

bool I2cHandlerRecoverBus(uint8_t port) {
  if (port >= HX_I2C_PORT_MAX) { return false; }

  g_bus_stats[port].recoveries_attempted++;
  HX_LOGW(TAG, "BUS%u recovery attempt", (unsigned)(port + 1));

  if (!I2cAdapterRecoverBus(port)) {
    HX_LOGE(TAG, "BUS%u recovery failed", (unsigned)(port + 1));
    return false;
  }

  g_bus_stats[port].recoveries_ok++;
  HX_LOGI(TAG, "BUS%u recovery OK — re-enabling all devices on port", (unsigned)(port + 1));

  // Re-enable all devices on this port so they get a clean chance
  // after the bus heals. The failure threshold will catch genuine
  // device failures within the next few polling cycles.
  RtosCriticalEnter(&g_registry_lock);
  for (int i = 0; i < HX_I2C_DEVICE_MAX; i++) {
    if (g_devices[i].active && g_devices[i].port == port) {
      g_devices[i].available           = true;
      g_devices[i].consecutive_failures = 0;
    }
  }
  RtosCriticalExit(&g_registry_lock);
  return true;
}

void I2cHandlerReenableDevice(HxI2cHandle handle) {
  HxI2cDevice* dev = device_from_handle(handle);
  if (!dev) { return; }
  RtosCriticalEnter(&g_registry_lock);
  dev->available            = true;
  dev->consecutive_failures = 0;
  RtosCriticalExit(&g_registry_lock);
  HX_LOGI(TAG, "BUS%u 0x%02x [%s] re-enabled",
          (unsigned)(dev->port + 1), (unsigned)dev->addr, dev->name);
}

// ---------------------------------------------------------------------------
// Statistics and introspection
// ---------------------------------------------------------------------------

bool I2cHandlerGetBusStats(uint8_t port, HxI2cBusStats* out) {
  if (!out || port >= HX_I2C_PORT_MAX) { return false; }
  *out = g_bus_stats[port];
  return true;
}

size_t I2cHandlerDeviceCount() {
  size_t count = 0;
  for (int i = 0; i < HX_I2C_DEVICE_MAX; i++) {
    if (g_devices[i].active) { count++; }
  }
  return count;
}

bool I2cHandlerGetDeviceAt(size_t index, HxI2cDeviceInfo* out) {
  if (!out) { return false; }
  size_t seen = 0;
  for (int i = 0; i < HX_I2C_DEVICE_MAX; i++) {
    if (!g_devices[i].active) { continue; }
    if (seen == index) {
      out->port                 = g_devices[i].port;
      out->addr                 = g_devices[i].addr;
      out->name                 = g_devices[i].name;
      out->available            = g_devices[i].available;
      out->tx_ok                = g_devices[i].tx_ok;
      out->tx_err               = g_devices[i].tx_err;
      out->rx_ok                = g_devices[i].rx_ok;
      out->rx_err               = g_devices[i].rx_err;
      out->consecutive_failures = g_devices[i].consecutive_failures;
      return true;
    }
    seen++;
  }
  return false;
}
