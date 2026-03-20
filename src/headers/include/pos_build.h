#pragma once

// ENV Definition
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  #define HX_TARGET_NAME "esp32p4"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HX_TARGET_NAME "esp32s3"
#else
  #define HX_TARGET_NAME "esp32"
#endif

// CONSOLE ADAPTER SELECTOR

#ifndef HX_BUILD_CONSOLE_ADAPTER
  #define HX_ENABLE_MODULE_CONSOLE      false
#endif

#if HX_ENABLE_MODULE_CONSOLE
#include "system/adapters/console_adapter.h"

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

#endif

// HANDLERS
#if HX_ENABLE_HANDLER_LITTLEFS
  #include "system/handlers/littlefs_handler.h"
#endif
