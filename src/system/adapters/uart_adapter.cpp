/*
  HexaOS - uart_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  UART bus adapter implementation using the IDF UART driver.
  RX and TX pins are resolved from the board pinmap at init time.
  RS485 half-duplex mode is enabled via IDF uart_set_mode when
  rs485_de_gpio >= 0 is passed to UartAdapterInit.

  Fixed frame parameters: 8 data bits, no parity, 1 stop bit, no
  hardware flow control. These cover all standard Modbus RTU, RS232,
  and generic serial device scenarios. Flow control support can be
  added to HxUartConfig in a future revision if required.
*/

#include "uart_adapter.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/soc_caps.h"

#include "headers/hx_pinfunc.h"
#include "system/core/log.h"
#include "system/core/pinmap.h"

static constexpr const char* TAG = "UART";

// ---------------------------------------------------------------------------
// Port mapping — logical index to IDF uart_port_t
// ---------------------------------------------------------------------------

// SOC_UART_NUM gives the total number of UART peripherals on the active chip.
// Logical ports without a matching peripheral map to -1.
static const uart_port_t kPortMap[HX_UART_PORT_MAX] = {
  UART_NUM_0,
  UART_NUM_1,
#if SOC_UART_NUM > 2
  UART_NUM_2,
#else
  (uart_port_t)-1,
#endif
#if SOC_UART_NUM > 3
  UART_NUM_3,
#else
  (uart_port_t)-1,
#endif
#if SOC_UART_NUM > 4
  UART_NUM_4,
#else
  (uart_port_t)-1,
#endif
};

// RX and TX pin function identifiers for each logical port.
static const HxPinFunction kRxFunc[HX_UART_PORT_MAX] = {
  HX_PIN_UART0_RX, HX_PIN_UART1_RX, HX_PIN_UART2_RX,
  HX_PIN_UART3_RX, HX_PIN_UART4_RX
};
static const HxPinFunction kTxFunc[HX_UART_PORT_MAX] = {
  HX_PIN_UART0_TX, HX_PIN_UART1_TX, HX_PIN_UART2_TX,
  HX_PIN_UART3_TX, HX_PIN_UART4_TX
};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static bool g_ready[HX_UART_PORT_MAX] = {};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static TickType_t resolve_write_timeout(uint32_t timeout_ms) {
  uint32_t t = (timeout_ms == 0) ? (uint32_t)HX_UART_DEFAULT_WRITE_TIMEOUT_MS : timeout_ms;
  return pdMS_TO_TICKS(t);
}

static uint32_t resolve_read_timeout(uint32_t timeout_ms) {
  return (timeout_ms == 0) ? (uint32_t)HX_UART_DEFAULT_READ_TIMEOUT_MS : timeout_ms;
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

bool UartAdapterInit(uint8_t port, uint32_t baud_rate, int rs485_de_gpio) {
  if (port >= HX_UART_PORT_MAX) { return false; }
  if (g_ready[port]) { return true; }

  if (kPortMap[port] == (uart_port_t)-1) {
    HX_LOGW(TAG, "UART%u not available on this chip", (unsigned)port);
    return false;
  }

  int16_t rx = PinmapGetGpioForFunction(kRxFunc[port]);
  int16_t tx = PinmapGetGpioForFunction(kTxFunc[port]);
  if (rx < 0 || tx < 0) {
    HX_LOGLL(TAG, "UART%u pins not mapped — skipping", (unsigned)port);
    return false;
  }

  uart_config_t uart_cfg = {};
  uart_cfg.baud_rate     = (int)baud_rate;
  uart_cfg.data_bits     = UART_DATA_8_BITS;
  uart_cfg.parity        = UART_PARITY_DISABLE;
  uart_cfg.stop_bits     = UART_STOP_BITS_1;
  uart_cfg.flow_ctrl     = UART_HW_FLOWCTRL_DISABLE;
  uart_cfg.source_clk    = UART_SCLK_DEFAULT;

  uart_port_t idf_port = kPortMap[port];

  esp_err_t err = uart_param_config(idf_port, &uart_cfg);
  if (err != ESP_OK) {
    HX_LOGE(TAG, "UART%u param config failed err=%d", (unsigned)port, (int)err);
    return false;
  }

  // RTS pin is used as the RS485 DE output when half-duplex mode is enabled.
  int rts_pin = (rs485_de_gpio >= 0) ? rs485_de_gpio : UART_PIN_NO_CHANGE;
  err = uart_set_pin(idf_port, (int)tx, (int)rx, rts_pin, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    HX_LOGE(TAG, "UART%u set pins failed err=%d rx=%d tx=%d",
            (unsigned)port, (int)err, (int)rx, (int)tx);
    return false;
  }

  err = uart_driver_install(idf_port,
                            HX_UART_RX_BUF_SIZE, HX_UART_TX_BUF_SIZE,
                            0, nullptr, 0);
  if (err != ESP_OK) {
    HX_LOGE(TAG, "UART%u driver install failed err=%d", (unsigned)port, (int)err);
    return false;
  }

  if (rs485_de_gpio >= 0) {
    err = uart_set_mode(idf_port, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) {
      HX_LOGE(TAG, "UART%u RS485 mode failed err=%d", (unsigned)port, (int)err);
      uart_driver_delete(idf_port);
      return false;
    }
    HX_LOGI(TAG, "UART%u ready baud=%lu rx=%d tx=%d de=%d (RS485)",
            (unsigned)port, (unsigned long)baud_rate, (int)rx, (int)tx, rs485_de_gpio);
  } else {
    HX_LOGI(TAG, "UART%u ready baud=%lu rx=%d tx=%d",
            (unsigned)port, (unsigned long)baud_rate, (int)rx, (int)tx);
  }

  g_ready[port] = true;
  return true;
}

bool UartAdapterReady(uint8_t port) {
  return (port < HX_UART_PORT_MAX) && g_ready[port];
}

bool UartAdapterPortMapped(uint8_t port) {
  if (port >= HX_UART_PORT_MAX) { return false; }
  if (kPortMap[port] == (uart_port_t)-1) { return false; }
  int16_t rx = PinmapGetGpioForFunction(kRxFunc[port]);
  int16_t tx = PinmapGetGpioForFunction(kTxFunc[port]);
  return (rx >= 0 && tx >= 0);
}

void UartAdapterDeinit(uint8_t port) {
  if (port >= HX_UART_PORT_MAX || !g_ready[port]) { return; }
  uart_driver_delete(kPortMap[port]);
  g_ready[port] = false;
}

bool UartAdapterWrite(uint8_t port,
                      const uint8_t* data, size_t len,
                      uint32_t timeout_ms) {
  if (!data || len == 0 || port >= HX_UART_PORT_MAX || !g_ready[port]) {
    return false;
  }
  int written = uart_write_bytes(kPortMap[port], (const char*)data, len);
  if (written < 0 || (size_t)written != len) { return false; }

  // Wait until the TX FIFO drains before returning — important for RS485
  // where the DE pin must not be deasserted while bytes are still in flight.
  return uart_wait_tx_done(kPortMap[port],
                           resolve_write_timeout(timeout_ms)) == ESP_OK;
}

size_t UartAdapterRead(uint8_t port,
                       uint8_t* buf, size_t max_len,
                       uint32_t timeout_ms) {
  if (!buf || max_len == 0 || port >= HX_UART_PORT_MAX || !g_ready[port]) {
    return 0;
  }
  int n = uart_read_bytes(kPortMap[port], buf, (uint32_t)max_len,
                          pdMS_TO_TICKS(resolve_read_timeout(timeout_ms)));
  return (n > 0) ? (size_t)n : 0;
}

size_t UartAdapterReadAvailable(uint8_t port) {
  if (port >= HX_UART_PORT_MAX || !g_ready[port]) { return 0; }
  size_t available = 0;
  uart_get_buffered_data_len(kPortMap[port], &available);
  return available;
}

void UartAdapterFlushRx(uint8_t port) {
  if (port >= HX_UART_PORT_MAX || !g_ready[port]) { return; }
  uart_flush_input(kPortMap[port]);
}
