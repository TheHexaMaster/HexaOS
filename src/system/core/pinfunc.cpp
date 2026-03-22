/*
  HexaOS - pinfunc.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Text helpers for stable logical pin function identifiers.
*/

#include "headers/hx_pinfunc.h"

const char* HxPinFunctionText(uint16_t pin_function) {
  switch (pin_function) {
    case HX_PIN_NONE: return "NONE";
    case HX_PIN_STATUS_LED: return "STATUS_LED";
    case HX_PIN_STATUS_LED_INV: return "STATUS_LED_INV";
    case HX_PIN_ACTIVITY_LED: return "ACTIVITY_LED";
    case HX_PIN_ACTIVITY_LED_INV: return "ACTIVITY_LED_INV";
    case HX_PIN_BOOT_BUTTON: return "BOOT_BUTTON";
    case HX_PIN_USER_BUTTON0: return "USER_BUTTON0";
    case HX_PIN_USER_BUTTON1: return "USER_BUTTON1";
    case HX_PIN_BUZZER: return "BUZZER";
    case HX_PIN_POWER_HOLD: return "POWER_HOLD";
    case HX_PIN_POWER_ENABLE: return "POWER_ENABLE";
    case HX_PIN_SYSTEM_RESET_IN: return "SYSTEM_RESET_IN";
    case HX_PIN_SYSTEM_RESET_OUT: return "SYSTEM_RESET_OUT";
    case HX_PIN_SYSTEM_IRQ0: return "SYSTEM_IRQ0";
    case HX_PIN_SYSTEM_IRQ1: return "SYSTEM_IRQ1";
    case HX_PIN_SYSTEM_IRQ2: return "SYSTEM_IRQ2";
    case HX_PIN_SYSTEM_IRQ3: return "SYSTEM_IRQ3";
    case HX_PIN_HEARTBEAT_OUT: return "HEARTBEAT_OUT";
    case HX_PIN_SAFEBOOT_REQUEST: return "SAFEBOOT_REQUEST";
    case HX_PIN_UART0_RX: return "UART0_RX";
    case HX_PIN_UART0_TX: return "UART0_TX";
    case HX_PIN_UART0_RTS: return "UART0_RTS";
    case HX_PIN_UART0_CTS: return "UART0_CTS";
    case HX_PIN_UART1_RX: return "UART1_RX";
    case HX_PIN_UART1_TX: return "UART1_TX";
    case HX_PIN_UART1_RTS: return "UART1_RTS";
    case HX_PIN_UART1_CTS: return "UART1_CTS";
    case HX_PIN_UART2_RX: return "UART2_RX";
    case HX_PIN_UART2_TX: return "UART2_TX";
    case HX_PIN_UART2_RTS: return "UART2_RTS";
    case HX_PIN_UART2_CTS: return "UART2_CTS";
    case HX_PIN_UART3_RX: return "UART3_RX";
    case HX_PIN_UART3_TX: return "UART3_TX";
    case HX_PIN_UART3_RTS: return "UART3_RTS";
    case HX_PIN_UART3_CTS: return "UART3_CTS";
    case HX_PIN_UART4_RX: return "UART4_RX";
    case HX_PIN_UART4_TX: return "UART4_TX";
    case HX_PIN_UART4_RTS: return "UART4_RTS";
    case HX_PIN_UART4_CTS: return "UART4_CTS";
    case HX_PIN_I2C0_SDA: return "I2C0_SDA";
    case HX_PIN_I2C0_SCL: return "I2C0_SCL";
    case HX_PIN_I2C1_SDA: return "I2C1_SDA";
    case HX_PIN_I2C1_SCL: return "I2C1_SCL";
    case HX_PIN_I2C2_SDA: return "I2C2_SDA";
    case HX_PIN_I2C2_SCL: return "I2C2_SCL";
    case HX_PIN_SPI0_MOSI: return "SPI0_MOSI";
    case HX_PIN_SPI0_MISO: return "SPI0_MISO";
    case HX_PIN_SPI0_SCLK: return "SPI0_SCLK";
    case HX_PIN_SPI1_MOSI: return "SPI1_MOSI";
    case HX_PIN_SPI1_MISO: return "SPI1_MISO";
    case HX_PIN_SPI1_SCLK: return "SPI1_SCLK";
    case HX_PIN_SPI2_MOSI: return "SPI2_MOSI";
    case HX_PIN_SPI2_MISO: return "SPI2_MISO";
    case HX_PIN_SPI2_SCLK: return "SPI2_SCLK";
    case HX_PIN_SPI3_MOSI: return "SPI3_MOSI";
    case HX_PIN_SPI3_MISO: return "SPI3_MISO";
    case HX_PIN_SPI3_SCLK: return "SPI3_SCLK";
    case HX_PIN_I2S0_MCLK: return "I2S0_MCLK";
    case HX_PIN_I2S0_BCLK: return "I2S0_BCLK";
    case HX_PIN_I2S0_WS: return "I2S0_WS";
    case HX_PIN_I2S0_DOUT: return "I2S0_DOUT";
    case HX_PIN_I2S0_DIN: return "I2S0_DIN";
    case HX_PIN_I2S1_MCLK: return "I2S1_MCLK";
    case HX_PIN_I2S1_BCLK: return "I2S1_BCLK";
    case HX_PIN_I2S1_WS: return "I2S1_WS";
    case HX_PIN_I2S1_DOUT: return "I2S1_DOUT";
    case HX_PIN_I2S1_DIN: return "I2S1_DIN";
    case HX_PIN_TWAI0_TX: return "TWAI0_TX";
    case HX_PIN_TWAI0_RX: return "TWAI0_RX";
    case HX_PIN_TWAI0_BO: return "TWAI0_BO";
    case HX_PIN_TWAI0_CLK: return "TWAI0_CLK";
    case HX_PIN_TWAI1_TX: return "TWAI1_TX";
    case HX_PIN_TWAI1_RX: return "TWAI1_RX";
    case HX_PIN_TWAI1_BO: return "TWAI1_BO";
    case HX_PIN_TWAI1_CLK: return "TWAI1_CLK";
    case HX_PIN_SDMMC0_CLK: return "SDMMC0_CLK";
    case HX_PIN_SDMMC0_CMD: return "SDMMC0_CMD";
    case HX_PIN_SDMMC0_D0: return "SDMMC0_D0";
    case HX_PIN_SDMMC0_D1: return "SDMMC0_D1";
    case HX_PIN_SDMMC0_D2: return "SDMMC0_D2";
    case HX_PIN_SDMMC0_D3: return "SDMMC0_D3";
    case HX_PIN_SDMMC0_D4: return "SDMMC0_D4";
    case HX_PIN_SDMMC0_D5: return "SDMMC0_D5";
    case HX_PIN_SDMMC0_D6: return "SDMMC0_D6";
    case HX_PIN_SDMMC0_D7: return "SDMMC0_D7";
    case HX_PIN_ETH0_MDC: return "ETH0_MDC";
    case HX_PIN_ETH0_MDIO: return "ETH0_MDIO";
    case HX_PIN_ETH0_POWER: return "ETH0_POWER";
    case HX_PIN_ETH0_RMII_TX_EN: return "ETH0_RMII_TX_EN";
    case HX_PIN_ETH0_RMII_TXD0: return "ETH0_RMII_TXD0";
    case HX_PIN_ETH0_RMII_TXD1: return "ETH0_RMII_TXD1";
    case HX_PIN_ETH0_RMII_RXD0: return "ETH0_RMII_RXD0";
    case HX_PIN_ETH0_RMII_RXD1: return "ETH0_RMII_RXD1";
    case HX_PIN_ETH0_RMII_CRS_DV: return "ETH0_RMII_CRS_DV";
    case HX_PIN_ETH0_RMII_REF_CLK: return "ETH0_RMII_REF_CLK";
    case HX_PIN_HOSTED0_SDIO_CLK: return "HOSTED0_SDIO_CLK";
    case HX_PIN_HOSTED0_SDIO_CMD: return "HOSTED0_SDIO_CMD";
    case HX_PIN_HOSTED0_SDIO_D0: return "HOSTED0_SDIO_D0";
    case HX_PIN_HOSTED0_SDIO_D1: return "HOSTED0_SDIO_D1";
    case HX_PIN_HOSTED0_SDIO_D2: return "HOSTED0_SDIO_D2";
    case HX_PIN_HOSTED0_SDIO_D3: return "HOSTED0_SDIO_D3";
    case HX_PIN_HOSTED0_RESET: return "HOSTED0_RESET";
    default: return "UNKNOWN";
  }
}
