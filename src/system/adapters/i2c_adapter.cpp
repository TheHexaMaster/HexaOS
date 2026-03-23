/*
  HexaOS - i2c_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  I2C bus adapter implementation using the IDF i2c_master driver (IDF 5.1+).
  SCL and SDA pins are resolved from the board pinmap at init time.
  Each logical port (0, 1, 2) maps to a chip I2C peripheral when available.
*/

#include "i2c_adapter.h"

#include "driver/i2c_master.h"
#include "soc/soc_caps.h"

#include "headers/hx_pinfunc.h"
#include "system/core/log.h"
#include "system/core/pinmap.h"

static constexpr const char* TAG = "I2C";

// ---------------------------------------------------------------------------
// Port mapping — logical index to IDF i2c_port_num_t
// ---------------------------------------------------------------------------

// SOC_I2C_NUM is defined by IDF and gives the total number of I2C peripherals
// on the active chip. Logical ports without a matching peripheral map to -1.
static const i2c_port_num_t kPortMap[HX_I2C_PORT_MAX] = {
  (i2c_port_num_t)0,                                        // I2C0 always present
#if SOC_I2C_NUM >= 2
  (i2c_port_num_t)1,
#else
  (i2c_port_num_t)-1,
#endif
#if SOC_I2C_NUM >= 3
  (i2c_port_num_t)2,
#else
  (i2c_port_num_t)-1,
#endif
};

// Pin function identifiers for each logical port.
static const HxPinFunction kSclFunc[HX_I2C_PORT_MAX] = {
  HX_PIN_I2C0_SCL, HX_PIN_I2C1_SCL, HX_PIN_I2C2_SCL
};
static const HxPinFunction kSdaFunc[HX_I2C_PORT_MAX] = {
  HX_PIN_I2C0_SDA, HX_PIN_I2C1_SDA, HX_PIN_I2C2_SDA
};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static i2c_master_bus_handle_t g_bus[HX_I2C_PORT_MAX] = {};
static bool                    g_ready[HX_I2C_PORT_MAX] = {};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static uint32_t resolve_timeout(uint32_t timeout_ms) {
  return (timeout_ms == 0) ? (uint32_t)HX_I2C_DEFAULT_TIMEOUT_MS : timeout_ms;
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

bool I2cAdapterBusInit(uint8_t port) {
  if (port >= HX_I2C_PORT_MAX) { return false; }
  if (g_ready[port]) { return true; }

  if (kPortMap[port] == (i2c_port_num_t)-1) {
    HX_LOGW(TAG, "BUS%u not available on this chip", (unsigned)(port + 1));
    return false;
  }

  int16_t scl = PinmapGetGpioForFunction(kSclFunc[port]);
  int16_t sda = PinmapGetGpioForFunction(kSdaFunc[port]);
  if (scl < 0 || sda < 0) {
    HX_LOGLL(TAG, "BUS%u pins not mapped — skipping", (unsigned)(port + 1));
    return false;
  }

  i2c_master_bus_config_t cfg          = {};
  cfg.clk_source                       = I2C_CLK_SRC_DEFAULT;
  cfg.i2c_port                         = kPortMap[port];
  cfg.scl_io_num                       = (gpio_num_t)scl;
  cfg.sda_io_num                       = (gpio_num_t)sda;
  cfg.glitch_ignore_cnt                = 7;
  cfg.flags.enable_internal_pullup     = true;

  esp_err_t err = i2c_new_master_bus(&cfg, &g_bus[port]);
  if (err != ESP_OK) {
    HX_LOGE(TAG, "BUS%u init failed err=%d scl=%d sda=%d",
            (unsigned)(port + 1), (int)err, (int)scl, (int)sda);
    return false;
  }

  g_ready[port] = true;
  HX_LOGI(TAG, "BUS%u ready scl=%d sda=%d", (unsigned)(port + 1), (int)scl, (int)sda);
  return true;
}

bool I2cAdapterBusReady(uint8_t port) {
  return (port < HX_I2C_PORT_MAX) && g_ready[port];
}

bool I2cAdapterAddDevice(uint8_t port, uint16_t addr, uint32_t freq_hz,
                         HxI2cDevHandle* out) {
  if (!out || port >= HX_I2C_PORT_MAX || !g_ready[port]) { return false; }

  i2c_device_config_t dev_cfg  = {};
  dev_cfg.dev_addr_length      = I2C_ADDR_BIT_LEN_7;
  dev_cfg.device_address       = addr;
  dev_cfg.scl_speed_hz         = freq_hz;

  i2c_master_dev_handle_t handle = nullptr;
  esp_err_t err = i2c_master_bus_add_device(g_bus[port], &dev_cfg, &handle);
  if (err != ESP_OK) {
    HX_LOGE(TAG, "BUS%u add device addr=0x%02x err=%d",
            (unsigned)(port + 1), (unsigned)addr, (int)err);
    return false;
  }

  *out = (HxI2cDevHandle)handle;
  return true;
}

void I2cAdapterRemoveDevice(HxI2cDevHandle handle) {
  if (!handle) { return; }
  i2c_master_bus_rm_device((i2c_master_dev_handle_t)handle);
}

bool I2cAdapterRecoverBus(uint8_t port) {
  if (port >= HX_I2C_PORT_MAX || !g_ready[port]) { return false; }
  return (i2c_master_bus_reset(g_bus[port]) == ESP_OK);
}

bool I2cAdapterWrite(HxI2cDevHandle handle,
                     const uint8_t* data, size_t len,
                     uint32_t timeout_ms) {
  if (!handle || !data || len == 0) { return false; }
  return i2c_master_transmit(
    (i2c_master_dev_handle_t)handle, data, len,
    (int)resolve_timeout(timeout_ms)) == ESP_OK;
}

bool I2cAdapterRead(HxI2cDevHandle handle,
                    uint8_t* buf, size_t len,
                    uint32_t timeout_ms) {
  if (!handle || !buf || len == 0) { return false; }
  return i2c_master_receive(
    (i2c_master_dev_handle_t)handle, buf, len,
    (int)resolve_timeout(timeout_ms)) == ESP_OK;
}

bool I2cAdapterWriteRead(HxI2cDevHandle handle,
                         const uint8_t* tx, size_t tx_len,
                         uint8_t* rx,       size_t rx_len,
                         uint32_t timeout_ms) {
  if (!handle || !tx || tx_len == 0 || !rx || rx_len == 0) { return false; }
  return i2c_master_transmit_receive(
    (i2c_master_dev_handle_t)handle,
    tx, tx_len, rx, rx_len,
    (int)resolve_timeout(timeout_ms)) == ESP_OK;
}

bool I2cAdapterProbe(uint8_t port, uint16_t addr, uint32_t timeout_ms) {
  if (port >= HX_I2C_PORT_MAX || !g_ready[port]) { return false; }
  uint32_t t = (timeout_ms == 0) ? (uint32_t)HX_I2C_PROBE_TIMEOUT_MS : timeout_ms;
  return (i2c_master_probe(g_bus[port], addr, (int)t) == ESP_OK);
}
