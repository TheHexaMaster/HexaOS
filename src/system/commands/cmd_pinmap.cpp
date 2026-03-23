/*
  HexaOS - cmd_pinmap.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Pinmap inspection commands for HexaOS.
  Registers: pinmap info, pinmap list, pinmap gpio, pinmap func, pinmap caps,
             pinmap bindings, pinmap validate, pinmap rawpinmap,
             pinmap rawbindings, pinmap raw.
*/

#include <stdio.h>
#include <string.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "system/core/config.h"
#include "system/core/pinmap.h"
#include "headers/hx_pinfunc.h"
#include "headers/hx_target_caps.h"

static void CmdAppendFlagText(char* out, size_t out_size, const char* text) {
  if (!out || (out_size == 0) || !text || !text[0]) {
    return;
  }

  size_t used = strlen(out);
  if (used >= (out_size - 1)) {
    return;
  }

  snprintf(out + used, out_size - used, "%s%s", (used > 0) ? "," : "", text);
}

static void CmdFormatGpioCaps(char* out, size_t out_size, uint16_t caps) {
  if (!out || (out_size == 0)) {
    return;
  }

  out[0] = '\0';
  if (caps == 0) {
    snprintf(out, out_size, "none");
    return;
  }

  if (caps & HX_GPIO_CAP_VALID)      { CmdAppendFlagText(out, out_size, "valid"); }
  if (caps & HX_GPIO_CAP_INPUT)      { CmdAppendFlagText(out, out_size, "input"); }
  if (caps & HX_GPIO_CAP_OUTPUT)     { CmdAppendFlagText(out, out_size, "output"); }
  if (caps & HX_GPIO_CAP_ANALOG)     { CmdAppendFlagText(out, out_size, "analog"); }
  if (caps & HX_GPIO_CAP_TOUCH)      { CmdAppendFlagText(out, out_size, "touch"); }
  if (caps & HX_GPIO_CAP_LOWPOWER)   { CmdAppendFlagText(out, out_size, "lowpower"); }
  if (caps & HX_GPIO_CAP_STRAP)      { CmdAppendFlagText(out, out_size, "strap"); }
  if (caps & HX_GPIO_CAP_FLASH)      { CmdAppendFlagText(out, out_size, "flash"); }
  if (caps & HX_GPIO_CAP_PSRAM)      { CmdAppendFlagText(out, out_size, "psram"); }
  if (caps & HX_GPIO_CAP_USB)        { CmdAppendFlagText(out, out_size, "usb"); }
  if (caps & HX_GPIO_CAP_INPUT_ONLY) { CmdAppendFlagText(out, out_size, "input_only"); }
  if (caps & HX_GPIO_CAP_DEFAULT_TX0){ CmdAppendFlagText(out, out_size, "default_tx0"); }
  if (caps & HX_GPIO_CAP_DEFAULT_RX0){ CmdAppendFlagText(out, out_size, "default_rx0"); }
}

static bool CmdParsePinFunctionToken(const char* token, uint16_t* out_pin_function) {
  if (!token || !token[0] || !out_pin_function) {
    return false;
  }

  char* endptr = nullptr;
  long numeric = strtol(token, &endptr, 10);
  if ((endptr != token) && (*endptr == '\0')) {
    if ((numeric < 0) || (numeric > HX_PINFUNC_MAX_ID)) {
      return false;
    }
    *out_pin_function = (uint16_t)numeric;
    return true;
  }

  return HxPinFunctionFromText(token, out_pin_function);
}

static HxCmdStatus CmdPinmapInfo(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: pinmap info");
    return HX_CMD_USAGE;
  }

  CmdOutPrintfLine(out, "ready = %s", PinmapIsReady() ? "true" : "false");
  CmdOutPrintfLine(out, "target = %s", HX_TARGET_NAME);
  CmdOutPrintfLine(out, "gpio_count = %u", (unsigned int)PinmapGpioCount());
  CmdOutPrintfLine(out, "mapped = %lu", (unsigned long)PinmapMappedCount());
  CmdOutPrintfLine(out, "bindings.i2c = %lu", (unsigned long)PinmapI2cBindingCount());
  CmdOutPrintfLine(out, "bindings.uart = %lu", (unsigned long)PinmapUartBindingCount());
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapList(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: pinmap list");
    return HX_CMD_USAGE;
  }

  uint8_t gpio_count = PinmapGpioCount();
  size_t shown = 0;
  for (uint8_t gpio = 0; gpio < gpio_count; gpio++) {
    uint16_t function_id = HX_PIN_NONE;
    if (!PinmapGetFunctionForGpio(gpio, &function_id)) {
      continue;
    }
    if (function_id == HX_PIN_NONE) {
      continue;
    }
    CmdOutPrintfLine(out, "gpio %u = %s (%u)", (unsigned int)gpio, HxPinFunctionText(function_id), (unsigned int)function_id);
    shown++;
  }

  if (shown == 0) {
    CmdOutWriteLine(out, "pinmap is empty");
  }
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapBindings(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: pinmap bindings");
    return HX_CMD_USAGE;
  }

  for (size_t i = 0; i < PinmapI2cBindingCount(); i++) {
    HxI2cDriverBinding binding{};
    if (!PinmapGetI2cBindingAt(i, &binding)) {
      continue;
    }
    CmdOutPrintfLine(out,
                     "i2c.%s.%u = port:%u address:%u",
                     binding.type,
                     (unsigned int)binding.instance,
                     (unsigned int)binding.port,
                     (unsigned int)binding.address);
  }

  for (size_t i = 0; i < PinmapUartBindingCount(); i++) {
    HxUartDriverBinding binding{};
    if (!PinmapGetUartBindingAt(i, &binding)) {
      continue;
    }
    CmdOutPrintfLine(out,
                     "uart.%s.%u = port:%d txen:%d re:%d de:%d",
                     binding.type,
                     (unsigned int)binding.instance,
                     (int)binding.uart_port,
                     (int)binding.txen_gpio,
                     (int)binding.re_gpio,
                     (int)binding.de_gpio);
  }

  if ((PinmapI2cBindingCount() == 0) && (PinmapUartBindingCount() == 0)) {
    CmdOutWriteLine(out, "bindings are empty");
  }
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapGpio(const char* args, HxCmdOutput* out) {
  int32_t gpio = -1;
  const char* cursor = CmdSkipWs(args);
  if (!CmdParseInt32Token(&cursor, &gpio) || (CmdSkipWs(cursor)[0] != '\0')) {
    CmdOutWriteLine(out, "usage: pinmap gpio <gpio>");
    return HX_CMD_USAGE;
  }

  if ((gpio < 0) || (gpio >= (int32_t)PinmapGpioCount())) {
    CmdOutWriteLine(out, "gpio out of range");
    return HX_CMD_ERROR;
  }

  uint16_t function_id = HX_PIN_NONE;
  bool mapped = PinmapGetFunctionForGpio((uint8_t)gpio, &function_id);
  uint16_t caps = PinmapGetGpioCaps((uint8_t)gpio);
  char caps_text[160];
  CmdFormatGpioCaps(caps_text, sizeof(caps_text), caps);

  CmdOutPrintfLine(out, "gpio = %ld", (long)gpio);
  CmdOutPrintfLine(out, "function = %s (%u)", mapped ? HxPinFunctionText(function_id) : "<unavailable>", mapped ? (unsigned int)function_id : 0u);
  CmdOutPrintfLine(out, "caps = 0x%04X (%s)", (unsigned int)caps, caps_text);
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapFunction(const char* args, HxCmdOutput* out) {
  char token[48];
  const char* cursor = CmdSkipWs(args);
  if (!CmdExtractToken(&cursor, token, sizeof(token)) || (CmdSkipWs(cursor)[0] != '\0')) {
    CmdOutWriteLine(out, "usage: pinmap func <function_id|HX_PIN_NAME|NAME>");
    return HX_CMD_USAGE;
  }

  uint16_t function_id = HX_PIN_NONE;
  if (!CmdParsePinFunctionToken(token, &function_id)) {
    CmdOutWriteLine(out, "invalid pin function");
    return HX_CMD_ERROR;
  }

  int16_t gpio = PinmapGetGpioForFunction(function_id);
  CmdOutPrintfLine(out, "function = %s (%u)", HxPinFunctionText(function_id), (unsigned int)function_id);
  if (gpio < 0) {
    CmdOutWriteLine(out, "gpio = <unmapped>");
  } else {
    CmdOutPrintfLine(out, "gpio = %d", (int)gpio);
  }
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapCaps(const char* args, HxCmdOutput* out) {
  int32_t gpio = -1;
  const char* cursor = CmdSkipWs(args);
  if (!CmdParseInt32Token(&cursor, &gpio) || (CmdSkipWs(cursor)[0] != '\0')) {
    CmdOutWriteLine(out, "usage: pinmap caps <gpio>");
    return HX_CMD_USAGE;
  }

  if ((gpio < 0) || (gpio >= (int32_t)PinmapGpioCount())) {
    CmdOutWriteLine(out, "gpio out of range");
    return HX_CMD_ERROR;
  }

  uint16_t caps = PinmapGetGpioCaps((uint8_t)gpio);
  char caps_text[160];
  CmdFormatGpioCaps(caps_text, sizeof(caps_text), caps);
  CmdOutPrintfLine(out, "gpio %ld caps = 0x%04X (%s)", (long)gpio, (unsigned int)caps, caps_text);
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapValidate(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: pinmap validate");
    return HX_CMD_USAGE;
  }

  bool ok = PinmapValidateCurrentConfig();
  CmdOutPrintfLine(out, "current pin config valid = %s", ok ? "true" : "false");
  return ok ? HX_CMD_OK : HX_CMD_ERROR;
}

static HxCmdStatus CmdPinmapRawPinmap(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: pinmap rawpinmap");
    return HX_CMD_USAGE;
  }

  CmdOutWriteLine(out, HxConfigData.board_pinmap);
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapRawBindings(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: pinmap rawbindings");
    return HX_CMD_USAGE;
  }

  CmdOutWriteLine(out, HxConfigData.drivers_bindings);
  return HX_CMD_OK;
}

static HxCmdStatus CmdPinmapRaw(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: pinmap raw");
    return HX_CMD_USAGE;
  }

  CmdOutWriteLine(out, HxConfigData.drivers_bindings);
  return HX_CMD_OK;
}

static const HxCmdDef kPinmapCommands[] = {
  { "pinmap info",        CmdPinmapInfo,        "Show target caps and pinmap summary" },
  { "pinmap list",        CmdPinmapList,        "List mapped GPIO logical functions" },
  { "pinmap gpio",        CmdPinmapGpio,        "Show logical function and caps for one GPIO" },
  { "pinmap func",        CmdPinmapFunction,    "Show GPIO mapped to one logical function" },
  { "pinmap caps",        CmdPinmapCaps,        "Show capability flags for one GPIO" },
  { "pinmap bindings",    CmdPinmapBindings,    "List typed driver bindings" },
  { "pinmap validate",    CmdPinmapValidate,    "Re-validate current pin config" },
  { "pinmap rawpinmap",   CmdPinmapRawPinmap,   "Show raw board.pinmap JSON" },
  { "pinmap rawbindings", CmdPinmapRawBindings, "Show raw drivers.bindings JSON" },
  { "pinmap raw",         CmdPinmapRaw,         "Alias for pinmap rawbindings" }
};

bool CommandRegisterPinmap() {
  for (size_t i = 0; i < (sizeof(kPinmapCommands) / sizeof(kPinmapCommands[0])); i++) {
    if (!CommandRegister(&kPinmapCommands[i])) {
      return false;
    }
  }
  return true;
}
