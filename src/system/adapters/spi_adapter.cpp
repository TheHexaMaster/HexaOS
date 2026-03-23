/*
  HexaOS - spi_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  SPI bus adapter implementation using the IDF spi_master driver.
  MOSI, MISO, and SCLK pins are resolved from the board pinmap at init time.
  Each logical port (0, 1) maps to SPI2_HOST and SPI3_HOST respectively.
  SPI3_HOST is guarded by SOC_SPI_PERIPH_NUM and skipped on chips that
  expose only SPI2 to the user.
*/

#include "spi_adapter.h"

#include "driver/spi_master.h"
#include "soc/soc_caps.h"

#include "headers/hx_pinfunc.h"
#include "system/core/log.h"
#include "system/core/pinmap.h"

static constexpr const char* TAG = "SPI";

// ---------------------------------------------------------------------------
// Port mapping — logical index to IDF spi_host_device_t
// ---------------------------------------------------------------------------

// SPI0 and SPI1 are reserved for internal flash on all ESP32 variants.
// User-accessible buses start at SPI2_HOST. SPI3_HOST is available on chips
// with SOC_SPI_PERIPH_NUM >= 3 (classic ESP32, S2, S3, P4).
// Logical port 0 → SPI2_HOST, logical port 1 → SPI3_HOST (if present).
static const spi_host_device_t kHostMap[HX_SPI_PORT_MAX] = {
  SPI2_HOST,
#if SOC_SPI_PERIPH_NUM >= 3
  SPI3_HOST,
#else
  (spi_host_device_t)-1,
#endif
};

// MOSI, MISO, SCLK pin function identifiers for each logical port.
static const HxPinFunction kMosiFunc[HX_SPI_PORT_MAX] = {
  HX_PIN_SPI0_MOSI, HX_PIN_SPI1_MOSI
};
static const HxPinFunction kMisoFunc[HX_SPI_PORT_MAX] = {
  HX_PIN_SPI0_MISO, HX_PIN_SPI1_MISO
};
static const HxPinFunction kSclkFunc[HX_SPI_PORT_MAX] = {
  HX_PIN_SPI0_SCLK, HX_PIN_SPI1_SCLK
};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static bool g_ready[HX_SPI_PORT_MAX] = {};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static uint32_t resolve_timeout(uint32_t timeout_ms) {
  return (timeout_ms == 0) ? (uint32_t)HX_SPI_DEFAULT_TIMEOUT_MS : timeout_ms;
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

bool SpiAdapterBusInit(uint8_t port) {
  if (port >= HX_SPI_PORT_MAX) { return false; }
  if (g_ready[port]) { return true; }

  if (kHostMap[port] == (spi_host_device_t)-1) {
    HX_LOGW(TAG, "SPI%u not available on this chip", (unsigned)port);
    return false;
  }

  int16_t mosi = PinmapGetGpioForFunction(kMosiFunc[port]);
  int16_t miso = PinmapGetGpioForFunction(kMisoFunc[port]);
  int16_t sclk = PinmapGetGpioForFunction(kSclkFunc[port]);

  if (mosi < 0 || sclk < 0) {
    HX_LOGLL(TAG, "SPI%u pins not mapped — skipping", (unsigned)port);
    return false;
  }

  // MISO is optional. Some buses are write-only (e.g. display-only).
  if (miso < 0) {
    HX_LOGLL(TAG, "SPI%u MISO not mapped — TX-only mode", (unsigned)port);
  }

  spi_bus_config_t cfg    = {};
  cfg.mosi_io_num         = mosi;
  cfg.miso_io_num         = (miso >= 0) ? miso : -1;
  cfg.sclk_io_num         = sclk;
  cfg.quadwp_io_num       = -1;
  cfg.quadhd_io_num       = -1;
  cfg.max_transfer_sz     = HX_SPI_MAX_TRANSFER_SIZE;

  esp_err_t err = spi_bus_initialize(kHostMap[port], &cfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    HX_LOGE(TAG, "SPI%u init failed err=%d mosi=%d miso=%d sclk=%d",
            (unsigned)port, (int)err, (int)mosi, (int)miso, (int)sclk);
    return false;
  }

  g_ready[port] = true;
  HX_LOGI(TAG, "SPI%u ready mosi=%d miso=%d sclk=%d",
          (unsigned)port, (int)mosi, (int)miso, (int)sclk);
  return true;
}

bool SpiAdapterBusReady(uint8_t port) {
  return (port < HX_SPI_PORT_MAX) && g_ready[port];
}

bool SpiAdapterAddDevice(uint8_t port, int cs_pin, uint32_t freq_hz,
                         uint8_t mode, HxSpiDevHandle* out) {
  if (!out || port >= HX_SPI_PORT_MAX || !g_ready[port]) { return false; }

  spi_device_interface_config_t dev_cfg = {};
  dev_cfg.mode                          = mode;
  dev_cfg.clock_speed_hz                = (int)freq_hz;
  dev_cfg.spics_io_num                  = cs_pin;
  dev_cfg.queue_size                    = 4;

  spi_device_handle_t handle = nullptr;
  esp_err_t err = spi_bus_add_device(kHostMap[port], &dev_cfg, &handle);
  if (err != ESP_OK) {
    HX_LOGE(TAG, "SPI%u add device cs=%d err=%d",
            (unsigned)port, cs_pin, (int)err);
    return false;
  }

  *out = (HxSpiDevHandle)handle;
  return true;
}

void SpiAdapterRemoveDevice(HxSpiDevHandle handle) {
  if (!handle) { return; }
  spi_bus_remove_device((spi_device_handle_t)handle);
}

bool SpiAdapterTransfer(HxSpiDevHandle handle,
                        const uint8_t* tx, uint8_t* rx, size_t len,
                        uint32_t timeout_ms) {
  if (!handle || len == 0) { return false; }

  spi_transaction_t t = {};
  t.length            = len * 8;  // IDF expects bits
  t.tx_buffer         = tx;
  t.rx_buffer         = rx;

  // Acquire/release ensures the device owns the bus for this transaction.
  spi_device_handle_t dev = (spi_device_handle_t)handle;
  if (spi_device_acquire_bus(dev, pdMS_TO_TICKS(resolve_timeout(timeout_ms))) != ESP_OK) {
    return false;
  }
  esp_err_t err = spi_device_polling_transmit(dev, &t);
  spi_device_release_bus(dev);
  return (err == ESP_OK);
}

bool SpiAdapterTransmit(HxSpiDevHandle handle,
                        const uint8_t* tx, size_t len,
                        uint32_t timeout_ms) {
  return SpiAdapterTransfer(handle, tx, nullptr, len, timeout_ms);
}
