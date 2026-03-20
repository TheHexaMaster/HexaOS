/*
  HexaOS - hx_build.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Build-time system identity and target selection definitions.
  Contains compile-time constants such as HexaOS name, version and active target platform so the firmware can expose a consistent identity at runtime.
*/

#pragma once

// CONSOLE ADAPTERS VARIANTS
#define HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC      1
#define HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG  2

// SYSTEM BUILD DEF

#define HX_SYSTEM_NAME                    "HexaOS"
#define HX_VERSION                        "0.0.1-alpha"
#define HX_BUILD_BOARD_ID                 "generic"
#define HX_BUILD_MODEL_ID                 "generic"
#define HX_CONFIG_DEFAULT_DEVICE_NAME      "HexaOS Device"

// MODULES
#define HX_ENABLE_MODULE_SYSTEM           true                                    // System module
#define HX_ENABLE_MODULE_STORAGE          false
#define HX_ENABLE_MODULE_BERRY            false
#define HX_ENABLE_MODULE_WEB              false
#define HX_ENABLE_MODULE_LVGL             false

// HANDLERS

#define HX_ENABLE_HANDLER_LITTLEFS        true   

// FEATURES
#define HX_BUILD_CONSOLE_ADAPTER          HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG  // HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG or HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC (IDF JTAG shall be more stable because of Arduino CDC bug causing random crash issue - MTVAL: 0x500d2000)
#define HX_ENABLE_FEATURE_WIFI            true
#define HX_ENABLE_FEATURE_ETH             true
#define HX_ENABLE_FEATURE_LITTLEFS        true
#define HX_CONFIG_DEFAULT_LOG_LEVEL        3                 // 0-err, 1-warn, 2-info, 3-debug   
#define HX_CONFIG_DEFAULT_SAFEBOOT_ENABLE  false
#define HX_CONFIG_DEFAULT_STATE_DELAY     2000









// ENV Definition
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  #define HX_TARGET_NAME "esp32p4"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HX_TARGET_NAME "esp32s3"
#else
  #define HX_TARGET_NAME "esp32"
#endif

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


