#pragma once

#define HX_NAME        "HexaOS"
#define HX_VERSION     "0.0.1-alpha"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
  #define HX_TARGET_NAME "esp32p4"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HX_TARGET_NAME "esp32s3"
#else
  #define HX_TARGET_NAME "esp32"
#endif