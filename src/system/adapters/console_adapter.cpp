/*
  HexaOS - console_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Console transport adapter implementation for HexaOS.
  Selects either Arduino USB CDC or the native ESP-IDF USB Serial/JTAG driver at build time and exposes one uniform API to the rest of the firmware.
*/

#include "console_adapter.h"

#include "headers/hx_build.h"
#include <string.h>

#if (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG)
  #include <esp_err.h>
  #include <driver/usb_serial_jtag.h>
  #include "system/core/rtos.h"
#endif

static bool g_console_adapter_ready = false;

bool ConsoleAdapterInit() {
  if (g_console_adapter_ready) {
    return true;
  }

#if (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC)
  Serial.begin(115200);
  delay(50);
  g_console_adapter_ready = true;
  return true;
#elif (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG)
  usb_serial_jtag_driver_config_t cfg = {};
  cfg.rx_buffer_size = 256;
  cfg.tx_buffer_size = 256;

  esp_err_t err = usb_serial_jtag_driver_install(&cfg);
  if ((err == ESP_OK) || (err == ESP_ERR_INVALID_STATE)) {
    g_console_adapter_ready = true;
    return true;
  }

  return false;
#else
  #error "Unsupported HX_BUILD_CONSOLE_ADAPTER selection"
#endif
}

int ConsoleAdapterReadByte() {
  if (!g_console_adapter_ready) {
    return -1;
  }

#if (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC)
  if (Serial.available() <= 0) {
    return -1;
  }
  return Serial.read();
#elif (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG)
  uint8_t ch = 0;
  int rd = usb_serial_jtag_read_bytes(&ch, 1, 0);
  if (rd == 1) {
    return (int)ch;
  }
  return -1;
#else
  return -1;
#endif
}

size_t ConsoleAdapterWriteData(const uint8_t* data, size_t len) {
  if (!g_console_adapter_ready || !data || (len == 0)) {
    return 0;
  }

#if (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC)
  return Serial.write(data, len);
#elif (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG)
  size_t total = 0;

  while (total < len) {
    int wr = usb_serial_jtag_write_bytes(data + total, len - total, RtosMsToTicks(20));
    if (wr <= 0) {
      break;
    }
    total += (size_t)wr;
  }

  usb_serial_jtag_wait_tx_done(RtosMsToTicks(20)); 
  return total;
#else
  return 0;
#endif
}

size_t ConsoleAdapterWriteText(const char* text) {
  if (!text) {
    return ConsoleAdapterWriteData((const uint8_t*)"", 0);
  }
  return ConsoleAdapterWriteData((const uint8_t*)text, strlen(text));
}

size_t ConsoleAdapterWriteChar(char ch) {
  return ConsoleAdapterWriteData((const uint8_t*)&ch, 1);
}

void ConsoleAdapterFlush() {
  if (!g_console_adapter_ready) {
    return;
  }

#if (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC)
  Serial.flush();
#elif (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_IDF_USB_SERIAL_JTAG)
  usb_serial_jtag_wait_tx_done(RtosMsToTicks(20));
#endif
}
