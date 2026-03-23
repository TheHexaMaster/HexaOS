/*
  HexaOS - hx_build.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Build-time system identity and target selection definitions.
  Contains compile-time constants such as HexaOS name, version and active target platform so the firmware can expose a consistent identity at runtime.
*/

#pragma once

#include "arduino.h"

// CONSOLE ADAPTERS VARIANTS
#define HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC      1
#define HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG  2

// SYSTEM BUILD DEF

#define HX_SYSTEM_NAME                    "HexaOS"
#define HX_VERSION                        "0.0.1"
#define HX_BUILD_BOARD_ID                 "generic"
#define HX_BUILD_MODEL_ID                 "generic"
#define HX_CONFIG_DEFAULT_DEVICE_NAME     "HexaOS Device"

// MODULES
#define HX_ENABLE_MODULE_STORAGE          true
#define HX_ENABLE_MODULE_I2C              true
#define HX_ENABLE_MODULE_SPI              true
#define HX_ENABLE_MODULE_UART             true
#define HX_ENABLE_MODULE_BERRY            false
#define HX_ENABLE_MODULE_WEB              false
#define HX_ENABLE_MODULE_LVGL             false

// PANIC ACTION
#define HX_PANIC_ACTION_HALT     1   // Halt in infinite loop (default, safe for debugging)
#define HX_PANIC_ACTION_RESTART  2   // Restart device after a short delay (for production builds)

#ifndef HX_PANIC_ACTION
  #define HX_PANIC_ACTION  HX_PANIC_ACTION_HALT
#endif

// FEATURES
#define HX_BUILD_CONSOLE_ADAPTER          HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG  // HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG or HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC (IDF JTAG shall be more stable because of Arduino CDC bug causing random crash issue - MTVAL: 0x500d2000)
#define HX_ENABLE_FEATURE_WIFI            true
#define HX_ENABLE_FEATURE_ETH             true
#define HX_ENABLE_FEATURE_LITTLEFS        true
#define HX_ENABLE_FEATURE_SD              true

// SDMMC slot and power configuration.
// These pick up board-level definitions (from pins_arduino.h) when present.
// Override in your board config before including this header if needed.
#ifndef HX_SDMMC_SLOT
  #ifdef BOARD_SDMMC_SLOT
    #define HX_SDMMC_SLOT BOARD_SDMMC_SLOT
  #else
    #define HX_SDMMC_SLOT 0
  #endif
#endif
#ifndef HX_SDMMC_POWER_ON_LEVEL
  #ifdef BOARD_SDMMC_POWER_ON_LEVEL
    #define HX_SDMMC_POWER_ON_LEVEL BOARD_SDMMC_POWER_ON_LEVEL
  #else
    #define HX_SDMMC_POWER_ON_LEVEL 0   // 0 = active-LOW (most common)
  #endif
#endif
#ifndef HX_SDMMC_POWER_CHANNEL
  #ifdef BOARD_SDMMC_POWER_CHANNEL
    #define HX_SDMMC_POWER_CHANNEL BOARD_SDMMC_POWER_CHANNEL
  #else
    #define HX_SDMMC_POWER_CHANNEL (-1)  // -1 = no on-chip LDO channel
  #endif
#endif
// I2C handler device registry capacity.
// Covers boards with many sensors across multiple I2C buses.
#ifndef HX_I2C_DEVICE_MAX
  #define HX_I2C_DEVICE_MAX 64
#endif

// Number of consecutive transaction failures before a device is marked
// unavailable. Prevents error log spam for disconnected sensors.
// Set to 0 to disable the availability policy.
#ifndef HX_I2C_DEVICE_FAILURE_THRESHOLD
  #define HX_I2C_DEVICE_FAILURE_THRESHOLD 5
#endif

// SPI handler device registry capacity.
#ifndef HX_SPI_DEVICE_MAX
  #define HX_SPI_DEVICE_MAX 32
#endif

// Consecutive SPI transaction failures before a device is marked unavailable.
// Set to 0 to disable the availability policy.
#ifndef HX_SPI_DEVICE_FAILURE_THRESHOLD
  #define HX_SPI_DEVICE_FAILURE_THRESHOLD 5
#endif

// SD card presence check interval used by the storage module scheduler.
#ifndef HX_STORAGE_SD_CHECK_INTERVAL_MS
  #define HX_STORAGE_SD_CHECK_INTERVAL_MS  3000
#endif

#define HX_CONFIG_DEFAULT_LOG_LEVEL        3                 // 0-err, 1-warn, 2-info, 3-debug, 4-lld
#define HX_CONFIG_DEFAULT_SAFEBOOT_ENABLE  false
#define HX_CONFIG_DEFAULT_STATE_DELAY     2000

// Build-generated JSON config lengths.
#define HX_BUILD_BOARD_PINMAP_MAX_LEN       1024
#define HX_BUILD_DRIVERS_BINDINGS_MAX_LEN   3072

// Optional pin override list.
// Each entry must be X(HX_PIN_..., gpio_number).
#ifndef HX_BUILD_PIN_OVERRIDE_LIST
  #define HX_BUILD_PIN_OVERRIDE_LIST(X)
#endif

// Driver family registries.
// Add only driver families that may appear in this build.
#ifndef HX_BUILD_I2C_DRIVER_TYPE_LIST
  #define HX_BUILD_I2C_DRIVER_TYPE_LIST(X) \
    X(DS3232) \
    X(HDC2010)
#endif

#ifndef HX_BUILD_UART_DRIVER_TYPE_LIST
  #define HX_BUILD_UART_DRIVER_TYPE_LIST(X) \
    X(RS485)
#endif

// DRIVERS DEFINITION TO BINDINGS JSON

#define HX_I2C_DRIVER_DS3232_0_ENABLED   1
#define HX_I2C_DRIVER_DS3232_0_PORT      0
#define HX_I2C_DRIVER_DS3232_0_ADDRESS   0x68

#define HX_I2C_DRIVER_HDC2010_0_ENABLED  1
#define HX_I2C_DRIVER_HDC2010_0_PORT     1
#define HX_I2C_DRIVER_HDC2010_0_ADDRESS  0x41

#define HX_I2C_DRIVER_HDC2010_1_ENABLED  1
#define HX_I2C_DRIVER_HDC2010_1_PORT     0
#define HX_I2C_DRIVER_HDC2010_1_ADDRESS  0x41

#define HX_I2C_DRIVER_HDC2010_2_ENABLED  1
#define HX_I2C_DRIVER_HDC2010_2_PORT     1
#define HX_I2C_DRIVER_HDC2010_2_ADDRESS  0x42

#define HX_I2C_DRIVER_HDC2010_3_ENABLED  1
#define HX_I2C_DRIVER_HDC2010_3_PORT     0
#define HX_I2C_DRIVER_HDC2010_3_ADDRESS  0x42

#define HX_UART_DRIVER_RS485_0_ENABLED    1
#define HX_UART_DRIVER_RS485_0_PORT       0
#define HX_UART_DRIVER_RS485_0_TXEN_GPIO  -1
#define HX_UART_DRIVER_RS485_0_RE_GPIO    -1
#define HX_UART_DRIVER_RS485_0_DE_GPIO    -1

// ENV Definition
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  #define HX_TARGET_NAME "esp32p4"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HX_TARGET_NAME "esp32s3"
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  #define HX_TARGET_NAME "esp32s2"
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  #define HX_TARGET_NAME "esp32c6"
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
  #define HX_TARGET_NAME "esp32c5"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  #define HX_TARGET_NAME "esp32c3"
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
  #define HX_TARGET_NAME "esp32c2"
#elif defined(CONFIG_IDF_TARGET_ESP32)
  #define HX_TARGET_NAME "esp32"
#else
  #define HX_TARGET_NAME "unknown"
#endif

#include "hx_build_layout_autogen.h"

// USER INTERFACE CONSOLE ADAPTER SELECTOR

  #if (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC)
    #ifndef ARDUINO_USB_MODE
      #define ARDUINO_USB_MODE 1
    #endif
    #ifndef ARDUINO_USB_CDC_ON_BOOT
      #define ARDUINO_USB_CDC_ON_BOOT 1
    #endif
  #else
    #ifndef ARDUINO_USB_CDC_ON_BOOT
      #define ARDUINO_USB_CDC_ON_BOOT 0
    #endif
  #endif
