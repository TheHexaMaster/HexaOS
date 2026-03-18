/*
  HexaOS - hx_build.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Build-time system identity and target selection definitions.
  Contains compile-time constants such as HexaOS name, version and active target platform so the firmware can expose a consistent identity at runtime.
*/


#pragma once

#include "hx_types.h"

#define HX_NAME    "HexaOS"
#define HX_VERSION "0.0.1-alpha"

#ifndef HX_BUILD_BOARD_ID
  #define HX_BUILD_BOARD_ID "generic"
#endif

#ifndef HX_BUILD_MODEL_ID
  #define HX_BUILD_MODEL_ID "generic"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32P4)
  #define HX_TARGET_NAME "esp32p4"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HX_TARGET_NAME "esp32s3"
#else
  #define HX_TARGET_NAME "esp32"
#endif

#ifndef HX_ENABLE_MODULE_SYSTEM
  #define HX_ENABLE_MODULE_SYSTEM 1
#endif

#ifndef HX_ENABLE_MODULE_CONSOLE
  #define HX_ENABLE_MODULE_CONSOLE 1
#endif

#ifndef HX_ENABLE_MODULE_STORAGE
  #define HX_ENABLE_MODULE_STORAGE 1
#endif

#ifndef HX_ENABLE_MODULE_BERRY
  #define HX_ENABLE_MODULE_BERRY 0
#endif

#ifndef HX_ENABLE_MODULE_WEB
  #define HX_ENABLE_MODULE_WEB 0
#endif

#ifndef HX_ENABLE_MODULE_LVGL
  #define HX_ENABLE_MODULE_LVGL 0
#endif

#ifndef HX_ENABLE_FEATURE_WIFI
  #define HX_ENABLE_FEATURE_WIFI 0
#endif

#ifndef HX_ENABLE_FEATURE_ETH
  #define HX_ENABLE_FEATURE_ETH 0
#endif

#ifndef HX_ENABLE_FEATURE_LITTLEFS
  #define HX_ENABLE_FEATURE_LITTLEFS 1
#endif

#ifndef HX_BUILD_DEFAULT_DEVICE_NAME
  #define HX_BUILD_DEFAULT_DEVICE_NAME "hexaos"
#endif

#ifndef HX_BUILD_DEFAULT_LOG_LEVEL
  #define HX_BUILD_DEFAULT_LOG_LEVEL HX_LOG_INFO
#endif

#ifndef HX_BUILD_DEFAULT_SAFEBOOT_ENABLE
  #define HX_BUILD_DEFAULT_SAFEBOOT_ENABLE 1
#endif