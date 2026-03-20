/*
  HexaOS - hx_build.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Build-time system identity and target selection definitions.
  Contains compile-time constants such as HexaOS name, version and active target platform so the firmware can expose a consistent identity at runtime.
*/

#pragma once

// SYSTEM BUILD DEF

#define HX_SYSTEM_NAME                    "HexaOS"
#define HX_VERSION                        "0.0.1-alpha"
#define HX_BUILD_BOARD_ID                 "generic"
#define HX_BUILD_MODEL_ID                 "generic"
#define HX_BUILD_DEFAULT_DEVICE_NAME      "HexaOS Device"

// MODULES
#define HX_ENABLE_MODULE_SYSTEM           true                                    // System module
#define HX_ENABLE_MODULE_CONSOLE          false                                   // Legacy module disabled. Interactive console moved into core user interface.

// USER INTERFACE
#define HX_ENABLE_CORE_USER_INTERFACE     true
  #define HX_BUILD_CONSOLE_ADAPTER        HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG  // HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG or HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC (IDF JTAG shall be more stable because of Arduino CDC bug causing random crash issue - MTVAL: 0x500d2000)
#define HX_ENABLE_MODULE_STORAGE          false
#define HX_ENABLE_MODULE_BERRY            false
#define HX_ENABLE_MODULE_WEB              false
#define HX_ENABLE_MODULE_LVGL             false

// States Settings



// CORES

#define HX_ENABLE_CORE_RTOS               true

// HANDLERS

#define HX_ENABLE_HANDLER_LITTLEFS        true   

// FEATURES

#define HX_ENABLE_FEATURE_WIFI            true
#define HX_ENABLE_FEATURE_ETH             true
#define HX_ENABLE_FEATURE_LITTLEFS        true
#define HX_BUILD_DEFAULT_LOG_LEVEL        HX_LOG_DEBUG            // Default LOG Level configuration.   
#define HX_BUILD_DEFAULT_SAFEBOOT_ENABLE  false
#define HX_CONFIG_DEFAULT_STATE_DELAY     2000



