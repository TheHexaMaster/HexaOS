/*
  HexaOS - pinmap.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core board pinmap and driver bindings service.
  Loads build-generated config defaults for board.pinmap and drivers.bindings,
  validates them against the active target capability database and materializes
  a fast runtime lookup model for GPIO-to-function, function-to-GPIO and
  typed driver binding access.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

struct HxI2cDriverBinding {
  char type[32];
  uint8_t instance;
  uint8_t port;
  uint16_t address;
};

struct HxUartDriverBinding {
  char type[32];
  uint8_t instance;
  int8_t uart_port;
  int8_t txen_gpio;
  int8_t re_gpio;
  int8_t de_gpio;
};

bool PinmapInit();
bool PinmapValidateCurrentConfig();
bool PinmapIsReady();
uint8_t PinmapGpioCount();
uint16_t PinmapGetGpioCaps(uint8_t gpio);
bool PinmapGetFunctionForGpio(uint8_t gpio, uint16_t* out_function);
int16_t PinmapGetGpioForFunction(uint16_t pin_function);
size_t PinmapMappedCount();

size_t PinmapI2cBindingCount();
size_t PinmapUartBindingCount();
bool PinmapGetI2cBindingAt(size_t index, HxI2cDriverBinding* out_binding);
bool PinmapGetUartBindingAt(size_t index, HxUartDriverBinding* out_binding);
bool PinmapFindI2cBinding(const char* type, uint8_t instance, HxI2cDriverBinding* out_binding);
bool PinmapFindUartBinding(const char* type, uint8_t instance, HxUartDriverBinding* out_binding);

const char* PinmapBindingsJson();
bool PinmapCopyBindingsJson(char* out, size_t out_size);
