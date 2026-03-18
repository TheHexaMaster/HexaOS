/*
  HexaOS - hx_build.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Build-time system identity and target selection definitions.
  Contains compile-time constants such as HexaOS name, version and active target platform so the firmware can expose a consistent identity at runtime.
*/


#pragma once

// ENV Definition
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  #define HX_TARGET_NAME "esp32p4"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HX_TARGET_NAME "esp32s3"
#else
  #define HX_TARGET_NAME "esp32"
#endif

// SYSTEM BUILD DEF

#define HX_SYSTEM_NAME                "HexaOS"
#define HX_VERSION                    "0.0.1-alpha"
#define HX_BUILD_BOARD_ID             "generic"
#define HX_BUILD_MODEL_ID             "generic"
#define HX_BUILD_DEFAULT_DEVICE_NAME  "HexaOS Device"

// MODULES
#define HX_ENABLE_MODULE_SYSTEM       true
#define HX_ENABLE_MODULE_CONSOLE      true
#define HX_ENABLE_MODULE_STORAGE      true
#define HX_ENABLE_MODULE_BERRY        true
#define HX_ENABLE_MODULE_WEB          true
#define HX_ENABLE_MODULE_LVGL         true

// FEATURES

#define HX_ENABLE_FEATURE_WIFI        true
#define HX_ENABLE_FEATURE_ETH         true
#define HX_ENABLE_FEATURE_LITTLEFS    true
#define HX_BUILD_DEFAULT_LOG_LEVEL    HX_LOG_INFO


#ifndef HX_BUILD_DEFAULT_SAFEBOOT_ENABLE
  #define HX_BUILD_DEFAULT_SAFEBOOT_ENABLE 1
#endif