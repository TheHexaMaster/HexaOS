/*
  HexaOS - hx_pinfunc.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Stable append-only registry of logical board pin functions.
  These identifiers do not assign physical GPIO numbers.
  They only define the canonical logical roles that may appear in
  board.pinmap defaults and in future runtime pinout configuration.
*/

#pragma once

#include <stdint.h>

// Pin function family ranges.
// Keep all numeric identifiers append-only forever.
#define HX_PIN_RANGE_COMMON_START   1
#define HX_PIN_RANGE_COMMON_END     99
#define HX_PIN_RANGE_UART_START     100
#define HX_PIN_RANGE_UART_END       199
#define HX_PIN_RANGE_I2C_START      200
#define HX_PIN_RANGE_I2C_END        299
#define HX_PIN_RANGE_SPI_START      300
#define HX_PIN_RANGE_SPI_END        399
#define HX_PIN_RANGE_I2S_START      400
#define HX_PIN_RANGE_I2S_END        499
#define HX_PIN_RANGE_TWAI_START     500
#define HX_PIN_RANGE_TWAI_END       599
#define HX_PIN_RANGE_SDMMC_START    600
#define HX_PIN_RANGE_SDMMC_END      699
#define HX_PIN_RANGE_ETH_START      700
#define HX_PIN_RANGE_ETH_END        799
#define HX_PIN_RANGE_HOSTED_START   800
#define HX_PIN_RANGE_HOSTED_END     899
#define HX_PINFUNC_MAX_ID           899

enum HxPinFunction : uint16_t {
  HX_PIN_NONE = 0,

  // Common board-level and system-level roles.
  HX_PIN_STATUS_LED        = 1,
  HX_PIN_STATUS_LED_INV    = 2,
  HX_PIN_ACTIVITY_LED      = 3,
  HX_PIN_ACTIVITY_LED_INV  = 4,
  HX_PIN_BOOT_BUTTON       = 5,
  HX_PIN_USER_BUTTON0      = 6,
  HX_PIN_USER_BUTTON1      = 7,
  HX_PIN_BUZZER            = 8,
  HX_PIN_POWER_HOLD        = 9,
  HX_PIN_POWER_ENABLE      = 10,
  HX_PIN_SYSTEM_RESET_IN   = 11,
  HX_PIN_SYSTEM_RESET_OUT  = 12,
  HX_PIN_SYSTEM_IRQ0       = 13,
  HX_PIN_SYSTEM_IRQ1       = 14,
  HX_PIN_SYSTEM_IRQ2       = 15,
  HX_PIN_SYSTEM_IRQ3       = 16,
  HX_PIN_HEARTBEAT_OUT     = 17,
  HX_PIN_SAFEBOOT_REQUEST  = 18,

  // UART family.
  HX_PIN_UART0_RX  = 100,
  HX_PIN_UART0_TX  = 101,
  HX_PIN_UART0_RTS = 102,
  HX_PIN_UART0_CTS = 103,
  HX_PIN_UART1_RX  = 104,
  HX_PIN_UART1_TX  = 105,
  HX_PIN_UART1_RTS = 106,
  HX_PIN_UART1_CTS = 107,
  HX_PIN_UART2_RX  = 108,
  HX_PIN_UART2_TX  = 109,
  HX_PIN_UART2_RTS = 110,
  HX_PIN_UART2_CTS = 111,
  HX_PIN_UART3_RX  = 112,
  HX_PIN_UART3_TX  = 113,
  HX_PIN_UART3_RTS = 114,
  HX_PIN_UART3_CTS = 115,
  HX_PIN_UART4_RX  = 116,
  HX_PIN_UART4_TX  = 117,
  HX_PIN_UART4_RTS = 118,
  HX_PIN_UART4_CTS = 119,

  // I2C family.
  HX_PIN_I2C0_SDA = 200,
  HX_PIN_I2C0_SCL = 201,
  HX_PIN_I2C1_SDA = 202,
  HX_PIN_I2C1_SCL = 203,
  HX_PIN_I2C2_SDA = 204,
  HX_PIN_I2C2_SCL = 205,

  // SPI family.
  HX_PIN_SPI0_MOSI = 300,
  HX_PIN_SPI0_MISO = 301,
  HX_PIN_SPI0_SCLK = 302,
  HX_PIN_SPI1_MOSI = 303,
  HX_PIN_SPI1_MISO = 304,
  HX_PIN_SPI1_SCLK = 305,
  HX_PIN_SPI2_MOSI = 306,
  HX_PIN_SPI2_MISO = 307,
  HX_PIN_SPI2_SCLK = 308,
  HX_PIN_SPI3_MOSI = 309,
  HX_PIN_SPI3_MISO = 310,
  HX_PIN_SPI3_SCLK = 311,

  // I2S family.
  HX_PIN_I2S0_MCLK = 400,
  HX_PIN_I2S0_BCLK = 401,
  HX_PIN_I2S0_WS   = 402,
  HX_PIN_I2S0_DOUT = 403,
  HX_PIN_I2S0_DIN  = 404,
  HX_PIN_I2S1_MCLK = 405,
  HX_PIN_I2S1_BCLK = 406,
  HX_PIN_I2S1_WS   = 407,
  HX_PIN_I2S1_DOUT = 408,
  HX_PIN_I2S1_DIN  = 409,

  // TWAI / CAN family.
  HX_PIN_TWAI0_TX  = 500,
  HX_PIN_TWAI0_RX  = 501,
  HX_PIN_TWAI0_BO  = 502,
  HX_PIN_TWAI0_CLK = 503,
  HX_PIN_TWAI1_TX  = 504,
  HX_PIN_TWAI1_RX  = 505,
  HX_PIN_TWAI1_BO  = 506,
  HX_PIN_TWAI1_CLK = 507,

  // SDMMC family.
  HX_PIN_SDMMC0_CLK = 600,
  HX_PIN_SDMMC0_CMD = 601,
  HX_PIN_SDMMC0_D0  = 602,
  HX_PIN_SDMMC0_D1  = 603,
  HX_PIN_SDMMC0_D2  = 604,
  HX_PIN_SDMMC0_D3  = 605,
  HX_PIN_SDMMC0_D4  = 606,
  HX_PIN_SDMMC0_D5  = 607,
  HX_PIN_SDMMC0_D6  = 608,
  HX_PIN_SDMMC0_D7  = 609,

  // Ethernet family.
  HX_PIN_ETH0_MDC          = 700,
  HX_PIN_ETH0_MDIO         = 701,
  HX_PIN_ETH0_POWER        = 702,
  HX_PIN_ETH0_RMII_TX_EN   = 703,
  HX_PIN_ETH0_RMII_TXD0    = 704,
  HX_PIN_ETH0_RMII_TXD1    = 705,
  HX_PIN_ETH0_RMII_RXD0    = 706,
  HX_PIN_ETH0_RMII_RXD1    = 707,
  HX_PIN_ETH0_RMII_CRS_DV  = 708,
  HX_PIN_ETH0_RMII_REF_CLK = 709,

  // ESP-Hosted / MCU-to-MCU SDIO family.
  HX_PIN_HOSTED0_SDIO_CLK   = 800,
  HX_PIN_HOSTED0_SDIO_CMD   = 801,
  HX_PIN_HOSTED0_SDIO_D0    = 802,
  HX_PIN_HOSTED0_SDIO_D1    = 803,
  HX_PIN_HOSTED0_SDIO_D2    = 804,
  HX_PIN_HOSTED0_SDIO_D3    = 805,
  HX_PIN_HOSTED0_RESET      = 806,
};

static inline bool HxPinFunctionIsNone(uint16_t pin_function) {
  return (pin_function == HX_PIN_NONE);
}

static inline bool HxPinFunctionIsCommon(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_COMMON_START) && (pin_function <= HX_PIN_RANGE_COMMON_END);
}

static inline bool HxPinFunctionIsUart(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_UART_START) && (pin_function <= HX_PIN_RANGE_UART_END);
}

static inline bool HxPinFunctionIsI2c(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_I2C_START) && (pin_function <= HX_PIN_RANGE_I2C_END);
}

static inline bool HxPinFunctionIsSpi(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_SPI_START) && (pin_function <= HX_PIN_RANGE_SPI_END);
}

static inline bool HxPinFunctionIsI2s(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_I2S_START) && (pin_function <= HX_PIN_RANGE_I2S_END);
}

static inline bool HxPinFunctionIsTwai(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_TWAI_START) && (pin_function <= HX_PIN_RANGE_TWAI_END);
}

static inline bool HxPinFunctionIsSdmmc(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_SDMMC_START) && (pin_function <= HX_PIN_RANGE_SDMMC_END);
}

static inline bool HxPinFunctionIsEthernet(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_ETH_START) && (pin_function <= HX_PIN_RANGE_ETH_END);
}

static inline bool HxPinFunctionIsHosted(uint16_t pin_function) {
  return (pin_function >= HX_PIN_RANGE_HOSTED_START) && (pin_function <= HX_PIN_RANGE_HOSTED_END);
}

const char* HxPinFunctionText(uint16_t pin_function);
