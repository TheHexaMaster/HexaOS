/*
  HexaOS - pinmap.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core board pinmap and driver bindings implementation.
  Parses the build-generated board.pinmap dense JSON array and the generic
  drivers.bindings JSON object from config, validates them against active
  target GPIO capability flags and builds reverse lookup tables and typed
  driver binding registries for runtime use.
*/

#include "pinmap.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers/hx_build.h"
#include "headers/hx_pinfunc.h"
#include "headers/hx_target_caps.h"
#include "system/core/config.h"
#include "system/core/log.h"
#include "system/core/runtime.h"

static constexpr uint8_t HX_PINMAP_MAX_GPIO = 64;
static constexpr uint8_t HX_PINMAP_JSON_MAX_DEPTH = 12;
static constexpr size_t HX_PINMAP_MAX_BINDING_TYPE_LEN = 31;
static constexpr size_t HX_PINMAP_MAX_I2C_BINDINGS = 32;
static constexpr size_t HX_PINMAP_MAX_UART_BINDINGS = 32;

#define HX_TARGET_GPIO_CAP_INIT(gpio_number, gpio_flags) gpio_flags,
static const uint16_t kHxTargetActiveGpioCaps[] = {
  HX_TARGET_GPIO_CAPS_CURRENT(HX_TARGET_GPIO_CAP_INIT)
};
#undef HX_TARGET_GPIO_CAP_INIT

static_assert((sizeof(kHxTargetActiveGpioCaps) / sizeof(kHxTargetActiveGpioCaps[0])) == HX_TARGET_CAPS_CURRENT_DEF.gpio_count,
              "Active target GPIO caps size mismatch");

static bool g_pinmap_ready = false;
static uint16_t g_gpio_to_function[HX_PINMAP_MAX_GPIO] = {0};
static int16_t g_function_to_gpio[HX_PINFUNC_MAX_ID + 1] = {0};
static char g_bindings_json[HX_BUILD_DRIVERS_BINDINGS_MAX_LEN + 1] = "{}";
static uint8_t g_mapped_count = 0;
static HxI2cDriverBinding g_i2c_bindings[HX_PINMAP_MAX_I2C_BINDINGS] = {};
static HxUartDriverBinding g_uart_bindings[HX_PINMAP_MAX_UART_BINDINGS] = {};
static size_t g_i2c_binding_count = 0;
static size_t g_uart_binding_count = 0;


#define HX_PINMAP_TYPE_NAME_INIT(type_name) #type_name,
static const char* const kHxBuildI2cDriverTypes[] = {
  HX_BUILD_I2C_DRIVER_TYPE_LIST(HX_PINMAP_TYPE_NAME_INIT)
  nullptr
};
static const char* const kHxBuildUartDriverTypes[] = {
  HX_BUILD_UART_DRIVER_TYPE_LIST(HX_PINMAP_TYPE_NAME_INIT)
  nullptr
};
#undef HX_PINMAP_TYPE_NAME_INIT

static bool PinmapTypeInRegistry(const char* type_name, const char* const* registry) {
  if (!type_name || !type_name[0] || !registry) {
    return false;
  }
  for (size_t i = 0; registry[i] != nullptr; i++) {
    if (strcmp(registry[i], type_name) == 0) {
      return true;
    }
  }
  return false;
}

static void PinmapResetState() {
  g_pinmap_ready = false;
  memset(g_gpio_to_function, 0, sizeof(g_gpio_to_function));
  for (size_t i = 0; i < (sizeof(g_function_to_gpio) / sizeof(g_function_to_gpio[0])); i++) {
    g_function_to_gpio[i] = -1;
  }
  memset(g_bindings_json, 0, sizeof(g_bindings_json));
  strcpy(g_bindings_json, "{}");
  g_mapped_count = 0;
  memset(g_i2c_bindings, 0, sizeof(g_i2c_bindings));
  memset(g_uart_bindings, 0, sizeof(g_uart_bindings));
  g_i2c_binding_count = 0;
  g_uart_binding_count = 0;
  Hx.pinmap_ready = false;
}

static uint16_t PinmapTargetCapFlags(uint8_t gpio) {
  if (gpio >= HX_TARGET_CAPS_CURRENT_DEF.gpio_count) {
    return 0;
  }
  return kHxTargetActiveGpioCaps[gpio];
}

static void JsonSkipWhitespace(const char** cursor) {
  if (!cursor || !(*cursor)) {
    return;
  }

  while ((**cursor != '\0') && isspace((unsigned char)**cursor)) {
    (*cursor)++;
  }
}

static bool JsonParseString(const char** cursor) {
  if (!cursor || !(*cursor) || (**cursor != '"')) {
    return false;
  }

  (*cursor)++;
  while (**cursor != '\0') {
    char ch = **cursor;
    if (ch == '"') {
      (*cursor)++;
      return true;
    }
    if (ch == '\\') {
      (*cursor)++;
      if (**cursor == '\0') {
        return false;
      }
      if (**cursor == 'u') {
        (*cursor)++;
        for (int i = 0; i < 4; i++) {
          char hex = **cursor;
          if (!isxdigit((unsigned char)hex)) {
            return false;
          }
          (*cursor)++;
        }
        continue;
      }
      (*cursor)++;
      continue;
    }
    if ((unsigned char)ch < 0x20U) {
      return false;
    }
    (*cursor)++;
  }

  return false;
}

static bool JsonParseStringCopy(const char** cursor, char* out_text, size_t out_size) {
  if (!cursor || !(*cursor) || !out_text || (out_size == 0) || (**cursor != '"')) {
    return false;
  }

  (*cursor)++;
  size_t used = 0;
  while (**cursor != '\0') {
    char ch = **cursor;
    if (ch == '"') {
      (*cursor)++;
      out_text[used] = '\0';
      return true;
    }
    if (ch == '\\') {
      (*cursor)++;
      if (**cursor == '\0') {
        return false;
      }
      char escaped = **cursor;
      if (escaped == 'u') {
        return false;
      }
      if (used + 1 >= out_size) {
        return false;
      }
      out_text[used++] = escaped;
      (*cursor)++;
      continue;
    }
    if ((unsigned char)ch < 0x20U) {
      return false;
    }
    if (used + 1 >= out_size) {
      return false;
    }
    out_text[used++] = ch;
    (*cursor)++;
  }

  return false;
}

static bool JsonParseNumber(const char** cursor) {
  if (!cursor || !(*cursor)) {
    return false;
  }

  const char* start = *cursor;
  if (**cursor == '-') {
    (*cursor)++;
  }

  if (**cursor == '0') {
    (*cursor)++;
  } else {
    if (!isdigit((unsigned char)**cursor)) {
      return false;
    }
    while (isdigit((unsigned char)**cursor)) {
      (*cursor)++;
    }
  }

  if (**cursor == '.') {
    (*cursor)++;
    if (!isdigit((unsigned char)**cursor)) {
      return false;
    }
    while (isdigit((unsigned char)**cursor)) {
      (*cursor)++;
    }
  }

  if ((**cursor == 'e') || (**cursor == 'E')) {
    (*cursor)++;
    if ((**cursor == '+') || (**cursor == '-')) {
      (*cursor)++;
    }
    if (!isdigit((unsigned char)**cursor)) {
      return false;
    }
    while (isdigit((unsigned char)**cursor)) {
      (*cursor)++;
    }
  }

  return (*cursor > start);
}

static bool JsonParseLiteral(const char** cursor, const char* literal) {
  if (!cursor || !(*cursor) || !literal) {
    return false;
  }

  size_t len = strlen(literal);
  if (strncmp(*cursor, literal, len) != 0) {
    return false;
  }
  *cursor += len;
  return true;
}

static bool JsonParseValue(const char** cursor, uint8_t depth);

static bool JsonParseArray(const char** cursor, uint8_t depth) {
  if (!cursor || !(*cursor) || (**cursor != '[') || (depth > HX_PINMAP_JSON_MAX_DEPTH)) {
    return false;
  }

  (*cursor)++;
  JsonSkipWhitespace(cursor);
  if (**cursor == ']') {
    (*cursor)++;
    return true;
  }

  while (**cursor != '\0') {
    if (!JsonParseValue(cursor, (uint8_t)(depth + 1U))) {
      return false;
    }
    JsonSkipWhitespace(cursor);
    if (**cursor == ',') {
      (*cursor)++;
      JsonSkipWhitespace(cursor);
      continue;
    }
    if (**cursor == ']') {
      (*cursor)++;
      return true;
    }
    return false;
  }

  return false;
}

static bool JsonParseObject(const char** cursor, uint8_t depth) {
  if (!cursor || !(*cursor) || (**cursor != '{') || (depth > HX_PINMAP_JSON_MAX_DEPTH)) {
    return false;
  }

  (*cursor)++;
  JsonSkipWhitespace(cursor);
  if (**cursor == '}') {
    (*cursor)++;
    return true;
  }

  while (**cursor != '\0') {
    if (!JsonParseString(cursor)) {
      return false;
    }
    JsonSkipWhitespace(cursor);
    if (**cursor != ':') {
      return false;
    }
    (*cursor)++;
    JsonSkipWhitespace(cursor);
    if (!JsonParseValue(cursor, (uint8_t)(depth + 1U))) {
      return false;
    }
    JsonSkipWhitespace(cursor);
    if (**cursor == ',') {
      (*cursor)++;
      JsonSkipWhitespace(cursor);
      continue;
    }
    if (**cursor == '}') {
      (*cursor)++;
      return true;
    }
    return false;
  }

  return false;
}

static bool JsonParseValue(const char** cursor, uint8_t depth) {
  if (!cursor || !(*cursor)) {
    return false;
  }

  JsonSkipWhitespace(cursor);
  switch (**cursor) {
    case '{':
      return JsonParseObject(cursor, depth);
    case '[':
      return JsonParseArray(cursor, depth);
    case '"':
      return JsonParseString(cursor);
    case 't':
      return JsonParseLiteral(cursor, "true");
    case 'f':
      return JsonParseLiteral(cursor, "false");
    case 'n':
      return JsonParseLiteral(cursor, "null");
    default:
      return JsonParseNumber(cursor);
  }
}

static bool JsonValidateRootObject(const char* json_text) {
  if (!json_text) {
    return false;
  }

  const char* cursor = json_text;
  JsonSkipWhitespace(&cursor);
  if (!JsonParseObject(&cursor, 0)) {
    return false;
  }
  JsonSkipWhitespace(&cursor);
  return (*cursor == '\0');
}

static bool PinmapParseUint16(const char** cursor, uint16_t* out_value) {
  if (!cursor || !(*cursor) || !out_value) {
    return false;
  }

  JsonSkipWhitespace(cursor);
  if (!isdigit((unsigned char)**cursor)) {
    return false;
  }

  char* end_ptr = nullptr;
  unsigned long raw = strtoul(*cursor, &end_ptr, 10);
  if ((end_ptr == *cursor) || (raw > 65535UL)) {
    return false;
  }

  *out_value = (uint16_t)raw;
  *cursor = end_ptr;
  return true;
}

static bool JsonParseInt32Strict(const char** cursor, int32_t* out_value) {
  if (!cursor || !(*cursor) || !out_value) {
    return false;
  }

  JsonSkipWhitespace(cursor);
  char* end_ptr = nullptr;
  long raw = strtol(*cursor, &end_ptr, 10);
  if (end_ptr == *cursor) {
    return false;
  }
  *out_value = (int32_t)raw;
  *cursor = end_ptr;
  return true;
}

static bool JsonExpectChar(const char** cursor, char expected) {
  if (!cursor || !(*cursor)) {
    return false;
  }
  JsonSkipWhitespace(cursor);
  if (**cursor != expected) {
    return false;
  }
  (*cursor)++;
  return true;
}

static bool JsonParseInstanceKey(const char** cursor, uint8_t* out_instance) {
  char key_text[12];
  if (!cursor || !out_instance) {
    return false;
  }
  JsonSkipWhitespace(cursor);
  if (!JsonParseStringCopy(cursor, key_text, sizeof(key_text))) {
    return false;
  }
  char* end_ptr = nullptr;
  unsigned long raw = strtoul(key_text, &end_ptr, 10);
  if ((end_ptr == key_text) || (*end_ptr != '\0') || (raw > 255UL)) {
    return false;
  }
  *out_instance = (uint8_t)raw;
  return true;
}

static uint16_t PinmapRequiredCaps(uint16_t pin_function) {
  switch (pin_function) {
    case HX_PIN_BOOT_BUTTON:
    case HX_PIN_USER_BUTTON0:
    case HX_PIN_USER_BUTTON1:
    case HX_PIN_SYSTEM_RESET_IN:
    case HX_PIN_SYSTEM_IRQ0:
    case HX_PIN_SYSTEM_IRQ1:
    case HX_PIN_SYSTEM_IRQ2:
    case HX_PIN_SYSTEM_IRQ3:
    case HX_PIN_SAFEBOOT_REQUEST:
    case HX_PIN_UART0_RX:
    case HX_PIN_UART1_RX:
    case HX_PIN_UART2_RX:
    case HX_PIN_UART3_RX:
    case HX_PIN_UART4_RX:
    case HX_PIN_UART0_CTS:
    case HX_PIN_UART1_CTS:
    case HX_PIN_UART2_CTS:
    case HX_PIN_UART3_CTS:
    case HX_PIN_UART4_CTS:
      return HX_GPIO_CAP_INPUT;

    case HX_PIN_STATUS_LED:
    case HX_PIN_STATUS_LED_INV:
    case HX_PIN_ACTIVITY_LED:
    case HX_PIN_ACTIVITY_LED_INV:
    case HX_PIN_BUZZER:
    case HX_PIN_POWER_HOLD:
    case HX_PIN_POWER_ENABLE:
    case HX_PIN_SYSTEM_RESET_OUT:
    case HX_PIN_HEARTBEAT_OUT:
    case HX_PIN_UART0_TX:
    case HX_PIN_UART1_TX:
    case HX_PIN_UART2_TX:
    case HX_PIN_UART3_TX:
    case HX_PIN_UART4_TX:
    case HX_PIN_UART0_RTS:
    case HX_PIN_UART1_RTS:
    case HX_PIN_UART2_RTS:
    case HX_PIN_UART3_RTS:
    case HX_PIN_UART4_RTS:
    case HX_PIN_SPI0_MOSI:
    case HX_PIN_SPI0_SCLK:
    case HX_PIN_SPI1_MOSI:
    case HX_PIN_SPI1_SCLK:
    case HX_PIN_SPI2_MOSI:
    case HX_PIN_SPI2_SCLK:
    case HX_PIN_SPI3_MOSI:
    case HX_PIN_SPI3_SCLK:
    case HX_PIN_I2S0_MCLK:
    case HX_PIN_I2S0_BCLK:
    case HX_PIN_I2S0_WS:
    case HX_PIN_I2S0_DOUT:
    case HX_PIN_I2S1_MCLK:
    case HX_PIN_I2S1_BCLK:
    case HX_PIN_I2S1_WS:
    case HX_PIN_I2S1_DOUT:
    case HX_PIN_TWAI0_TX:
    case HX_PIN_TWAI0_BO:
    case HX_PIN_TWAI0_CLK:
    case HX_PIN_TWAI1_TX:
    case HX_PIN_TWAI1_BO:
    case HX_PIN_TWAI1_CLK:
    case HX_PIN_ETH0_POWER:
    case HX_PIN_ETH0_RMII_TX_EN:
    case HX_PIN_ETH0_RMII_TXD0:
    case HX_PIN_ETH0_RMII_TXD1:
    case HX_PIN_HOSTED0_RESET:
      return HX_GPIO_CAP_OUTPUT;

    default:
      return (HX_GPIO_CAP_INPUT | HX_GPIO_CAP_OUTPUT);
  }
}

static bool PinmapValidateGpioAssignment(uint8_t gpio, uint16_t pin_function) {
  uint16_t caps = PinmapTargetCapFlags(gpio);
  if ((caps & HX_GPIO_CAP_VALID) == 0U) {
    HX_LOGE("PIN", "gpio=%u invalid for target", (unsigned int)gpio);
    return false;
  }
  if ((caps & HX_GPIO_CAP_FLASH) != 0U) {
    HX_LOGE("PIN", "gpio=%u reserved for flash/boot storage", (unsigned int)gpio);
    return false;
  }
  if ((caps & HX_GPIO_CAP_PSRAM) != 0U) {
    HX_LOGE("PIN", "gpio=%u reserved for psram", (unsigned int)gpio);
    return false;
  }

  uint16_t required = PinmapRequiredCaps(pin_function);
  if (((required & HX_GPIO_CAP_INPUT) != 0U) && ((caps & HX_GPIO_CAP_INPUT) == 0U)) {
    HX_LOGE("PIN", "gpio=%u missing input capability for function=%u", (unsigned int)gpio, (unsigned int)pin_function);
    return false;
  }
  if (((required & HX_GPIO_CAP_OUTPUT) != 0U) && ((caps & HX_GPIO_CAP_OUTPUT) == 0U)) {
    HX_LOGE("PIN", "gpio=%u missing output capability for function=%u", (unsigned int)gpio, (unsigned int)pin_function);
    return false;
  }
  if ((caps & HX_GPIO_CAP_STRAP) != 0U) {
    HX_LOGW("PIN", "gpio=%u is a strap pin for function=%u", (unsigned int)gpio, (unsigned int)pin_function);
  }
  return true;
}

static bool PinmapValidateHelperGpio(int32_t gpio, const char* owner_text) {
  if (!owner_text) {
    owner_text = "binding";
  }
  if (gpio < 0) {
    return true;
  }
  if (gpio >= (int32_t)HX_TARGET_CAPS_CURRENT_DEF.gpio_count) {
    HX_LOGE("PIN", "%s helper gpio=%ld out of range", owner_text, (long)gpio);
    return false;
  }
  uint16_t caps = PinmapTargetCapFlags((uint8_t)gpio);
  if ((caps & HX_GPIO_CAP_VALID) == 0U) {
    HX_LOGE("PIN", "%s helper gpio=%ld invalid", owner_text, (long)gpio);
    return false;
  }
  if ((caps & HX_GPIO_CAP_FLASH) != 0U) {
    HX_LOGE("PIN", "%s helper gpio=%ld reserved for flash/boot storage", owner_text, (long)gpio);
    return false;
  }
  if ((caps & HX_GPIO_CAP_PSRAM) != 0U) {
    HX_LOGE("PIN", "%s helper gpio=%ld reserved for psram", owner_text, (long)gpio);
    return false;
  }
  if ((caps & HX_GPIO_CAP_OUTPUT) == 0U) {
    HX_LOGE("PIN", "%s helper gpio=%ld missing output capability", owner_text, (long)gpio);
    return false;
  }
  if ((caps & HX_GPIO_CAP_STRAP) != 0U) {
    HX_LOGW("PIN", "%s helper gpio=%ld is a strap pin", owner_text, (long)gpio);
  }
  return true;
}

static bool PinmapParseDenseArray(const char* json_text, uint16_t* out_gpio_to_function, uint8_t* out_mapped_count) {
  if (!json_text || !out_gpio_to_function || !out_mapped_count) {
    return false;
  }

  *out_mapped_count = 0;
  memset(out_gpio_to_function, 0, sizeof(uint16_t) * HX_PINMAP_MAX_GPIO);

  const char* cursor = json_text;
  JsonSkipWhitespace(&cursor);
  if (*cursor != '[') {
    HX_LOGE("PIN", "board.pinmap must be a JSON array");
    return false;
  }
  cursor++;
  JsonSkipWhitespace(&cursor);

  uint8_t gpio_index = 0;
  if (*cursor == ']') {
    cursor++;
    JsonSkipWhitespace(&cursor);
    return (*cursor == '\0');
  }

  while (*cursor != '\0') {
    if (gpio_index >= HX_TARGET_CAPS_CURRENT_DEF.gpio_count) {
      HX_LOGE("PIN", "board.pinmap exceeds target gpio count=%u", (unsigned int)HX_TARGET_CAPS_CURRENT_DEF.gpio_count);
      return false;
    }

    uint16_t value = 0;
    if (!PinmapParseUint16(&cursor, &value)) {
      HX_LOGE("PIN", "board.pinmap invalid integer at gpio=%u", (unsigned int)gpio_index);
      return false;
    }
    if (value > HX_PINFUNC_MAX_ID) {
      HX_LOGE("PIN", "board.pinmap invalid function id=%u at gpio=%u", (unsigned int)value, (unsigned int)gpio_index);
      return false;
    }
    if ((value != HX_PIN_NONE) && !PinmapValidateGpioAssignment(gpio_index, value)) {
      return false;
    }

    out_gpio_to_function[gpio_index] = value;
    if (value != HX_PIN_NONE) {
      (*out_mapped_count)++;
    }

    JsonSkipWhitespace(&cursor);
    if (*cursor == ',') {
      cursor++;
      JsonSkipWhitespace(&cursor);
      gpio_index++;
      continue;
    }
    if (*cursor == ']') {
      cursor++;
      JsonSkipWhitespace(&cursor);
      if (*cursor != '\0') {
        HX_LOGE("PIN", "board.pinmap trailing data detected");
        return false;
      }
      return true;
    }

    HX_LOGE("PIN", "board.pinmap syntax error at gpio=%u", (unsigned int)gpio_index);
    return false;
  }

  return false;
}

static bool PinmapBuildReverseLookup(const uint16_t* gpio_to_function) {
  if (!gpio_to_function) {
    return false;
  }

  for (size_t i = 0; i < (sizeof(g_function_to_gpio) / sizeof(g_function_to_gpio[0])); i++) {
    g_function_to_gpio[i] = -1;
  }

  for (uint8_t gpio = 0; gpio < HX_TARGET_CAPS_CURRENT_DEF.gpio_count; gpio++) {
    uint16_t pin_function = gpio_to_function[gpio];
    if (pin_function == HX_PIN_NONE) {
      continue;
    }
    if (g_function_to_gpio[pin_function] >= 0) {
      HX_LOGE("PIN", "duplicate function id=%u on gpio=%u and gpio=%d",
              (unsigned int)pin_function,
              (unsigned int)gpio,
              (int)g_function_to_gpio[pin_function]);
      return false;
    }
    g_function_to_gpio[pin_function] = (int16_t)gpio;
  }

  return true;
}


static uint16_t PinmapI2cSdaFunctionForPort(uint8_t port) {
  switch (port) {
    case 0: return HX_PIN_I2C0_SDA;
    case 1: return HX_PIN_I2C1_SDA;
    case 2: return HX_PIN_I2C2_SDA;
    default: return HX_PIN_NONE;
  }
}

static uint16_t PinmapI2cSclFunctionForPort(uint8_t port) {
  switch (port) {
    case 0: return HX_PIN_I2C0_SCL;
    case 1: return HX_PIN_I2C1_SCL;
    case 2: return HX_PIN_I2C2_SCL;
    default: return HX_PIN_NONE;
  }
}

static uint16_t PinmapUartTxFunctionForPort(uint8_t port) {
  switch (port) {
    case 0: return HX_PIN_UART0_TX;
    case 1: return HX_PIN_UART1_TX;
    case 2: return HX_PIN_UART2_TX;
    case 3: return HX_PIN_UART3_TX;
    case 4: return HX_PIN_UART4_TX;
    default: return HX_PIN_NONE;
  }
}

static uint16_t PinmapUartRxFunctionForPort(uint8_t port) {
  switch (port) {
    case 0: return HX_PIN_UART0_RX;
    case 1: return HX_PIN_UART1_RX;
    case 2: return HX_PIN_UART2_RX;
    case 3: return HX_PIN_UART3_RX;
    case 4: return HX_PIN_UART4_RX;
    default: return HX_PIN_NONE;
  }
}

static bool PinmapStoreI2cBinding(const char* type_name, uint8_t instance, uint8_t port, uint16_t address) {
  if (!type_name || !type_name[0]) {
    HX_LOGE("PIN", "i2c binding missing type");
    return false;
  }
  if (g_i2c_binding_count >= HX_PINMAP_MAX_I2C_BINDINGS) {
    HX_LOGE("PIN", "too many i2c bindings");
    return false;
  }
  for (size_t i = 0; i < g_i2c_binding_count; i++) {
    if ((strcmp(g_i2c_bindings[i].type, type_name) == 0) && (g_i2c_bindings[i].instance == instance)) {
      HX_LOGE("PIN", "duplicate i2c binding type=%s instance=%u", type_name, (unsigned int)instance);
      return false;
    }
  }
  HxI2cDriverBinding* entry = &g_i2c_bindings[g_i2c_binding_count++];
  snprintf(entry->type, sizeof(entry->type), "%s", type_name);
  entry->instance = instance;
  entry->port = port;
  entry->address = address;
  return true;
}

static bool PinmapStoreUartBinding(const char* type_name, uint8_t instance, int8_t uart_port, int8_t txen_gpio, int8_t re_gpio, int8_t de_gpio) {
  if (!type_name || !type_name[0]) {
    HX_LOGE("PIN", "uart binding missing type");
    return false;
  }
  if (g_uart_binding_count >= HX_PINMAP_MAX_UART_BINDINGS) {
    HX_LOGE("PIN", "too many uart bindings");
    return false;
  }
  for (size_t i = 0; i < g_uart_binding_count; i++) {
    if ((strcmp(g_uart_bindings[i].type, type_name) == 0) && (g_uart_bindings[i].instance == instance)) {
      HX_LOGE("PIN", "duplicate uart binding type=%s instance=%u", type_name, (unsigned int)instance);
      return false;
    }
  }

  char owner_text[80];
  snprintf(owner_text, sizeof(owner_text), "%s.%u", type_name, (unsigned int)instance);
  if (!PinmapValidateHelperGpio(txen_gpio, owner_text)) {
    return false;
  }
  if (!PinmapValidateHelperGpio(re_gpio, owner_text)) {
    return false;
  }
  if (!PinmapValidateHelperGpio(de_gpio, owner_text)) {
    return false;
  }

  HxUartDriverBinding* entry = &g_uart_bindings[g_uart_binding_count++];
  snprintf(entry->type, sizeof(entry->type), "%s", type_name);
  entry->instance = instance;
  entry->uart_port = uart_port;
  entry->txen_gpio = txen_gpio;
  entry->re_gpio = re_gpio;
  entry->de_gpio = de_gpio;
  return true;
}

static bool PinmapParseI2cInstanceObject(const char** cursor, const char* type_name, uint8_t instance) {
  if (!JsonExpectChar(cursor, '{')) {
    return false;
  }

  int32_t port = -1;
  int32_t address = -1;

  JsonSkipWhitespace(cursor);
  if (**cursor == '}') {
    return false;
  }

  while (**cursor != '\0') {
    char key[16];
    if (!JsonParseStringCopy(cursor, key, sizeof(key))) {
      return false;
    }
    if (!JsonExpectChar(cursor, ':')) {
      return false;
    }

    if (strcmp(key, "i2c") == 0) {
      if (!JsonParseInt32Strict(cursor, &port)) {
        return false;
      }
    } else if (strcmp(key, "address") == 0) {
      if (!JsonParseInt32Strict(cursor, &address)) {
        return false;
      }
    } else {
      if (!JsonParseValue(cursor, 1)) {
        return false;
      }
    }

    JsonSkipWhitespace(cursor);
    if (**cursor == ',') {
      (*cursor)++;
      JsonSkipWhitespace(cursor);
      continue;
    }
    if (**cursor == '}') {
      (*cursor)++;
      break;
    }
    return false;
  }

  if ((port < 0) || (port > 255) || (address < 0) || (address > 65535)) {
    return false;
  }

  return PinmapStoreI2cBinding(type_name, instance, (uint8_t)port, (uint16_t)address);
}

static bool PinmapParseI2cTypeObject(const char** cursor, const char* type_name) {
  if (!JsonExpectChar(cursor, '{')) {
    return false;
  }
  JsonSkipWhitespace(cursor);
  if (**cursor == '}') {
    (*cursor)++;
    return true;
  }
  while (**cursor != '\0') {
    uint8_t instance = 0;
    if (!JsonParseInstanceKey(cursor, &instance)) {
      return false;
    }
    if (!JsonExpectChar(cursor, ':')) {
      return false;
    }
    if (!PinmapParseI2cInstanceObject(cursor, type_name, instance)) {
      return false;
    }
    JsonSkipWhitespace(cursor);
    if (**cursor == ',') {
      (*cursor)++;
      JsonSkipWhitespace(cursor);
      continue;
    }
    if (**cursor == '}') {
      (*cursor)++;
      return true;
    }
    return false;
  }
  return false;
}

static bool PinmapParseUartInstanceObject(const char** cursor, const char* type_name, uint8_t instance) {
  if (!JsonExpectChar(cursor, '{')) {
    return false;
  }

  int32_t uart = -1;
  int32_t txen = -1;
  int32_t re = -1;
  int32_t de = -1;

  JsonSkipWhitespace(cursor);
  if (**cursor == '}') {
    return false;
  }

  while (**cursor != '\0') {
    char key[16];
    if (!JsonParseStringCopy(cursor, key, sizeof(key))) {
      return false;
    }
    if (!JsonExpectChar(cursor, ':')) {
      return false;
    }
    if (strcmp(key, "uart") == 0) {
      if (!JsonParseInt32Strict(cursor, &uart)) {
        return false;
      }
    } else if (strcmp(key, "txen") == 0) {
      if (!JsonParseInt32Strict(cursor, &txen)) {
        return false;
      }
    } else if (strcmp(key, "re") == 0) {
      if (!JsonParseInt32Strict(cursor, &re)) {
        return false;
      }
    } else if (strcmp(key, "de") == 0) {
      if (!JsonParseInt32Strict(cursor, &de)) {
        return false;
      }
    } else {
      if (!JsonParseValue(cursor, 1)) {
        return false;
      }
    }

    JsonSkipWhitespace(cursor);
    if (**cursor == ',') {
      (*cursor)++;
      JsonSkipWhitespace(cursor);
      continue;
    }
    if (**cursor == '}') {
      (*cursor)++;
      break;
    }
    return false;
  }

  if ((uart < 0) || (uart > 127)) {
    return false;
  }

  return PinmapStoreUartBinding(type_name, instance, (int8_t)uart, (int8_t)txen, (int8_t)re, (int8_t)de);
}

static bool PinmapParseUartTypeObject(const char** cursor, const char* type_name) {
  if (!JsonExpectChar(cursor, '{')) {
    return false;
  }
  JsonSkipWhitespace(cursor);
  if (**cursor == '}') {
    (*cursor)++;
    return true;
  }
  while (**cursor != '\0') {
    uint8_t instance = 0;
    if (!JsonParseInstanceKey(cursor, &instance)) {
      return false;
    }
    if (!JsonExpectChar(cursor, ':')) {
      return false;
    }
    if (!PinmapParseUartInstanceObject(cursor, type_name, instance)) {
      return false;
    }
    JsonSkipWhitespace(cursor);
    if (**cursor == ',') {
      (*cursor)++;
      JsonSkipWhitespace(cursor);
      continue;
    }
    if (**cursor == '}') {
      (*cursor)++;
      return true;
    }
    return false;
  }
  return false;
}

static bool PinmapParseBindingsObject(const char* json_text) {
  if (!json_text) {
    return false;
  }

  const char* cursor = json_text;
  JsonSkipWhitespace(&cursor);
  if (!JsonExpectChar(&cursor, '{')) {
    return false;
  }
  JsonSkipWhitespace(&cursor);
  if (*cursor == '}') {
    cursor++;
    JsonSkipWhitespace(&cursor);
    return (*cursor == '\0');
  }

  while (*cursor != '\0') {
    char type_name[HX_PINMAP_MAX_BINDING_TYPE_LEN + 1];
    if (!JsonParseStringCopy(&cursor, type_name, sizeof(type_name))) {
      return false;
    }
    if (!JsonExpectChar(&cursor, ':')) {
      return false;
    }

    if (PinmapTypeInRegistry(type_name, kHxBuildI2cDriverTypes)) {
      if (!PinmapParseI2cTypeObject(&cursor, type_name)) {
        return false;
      }
    } else if (PinmapTypeInRegistry(type_name, kHxBuildUartDriverTypes)) {
      if (!PinmapParseUartTypeObject(&cursor, type_name)) {
        return false;
      }
    } else {
      HX_LOGE("PIN", "drivers.bindings contains unknown or unregistered driver type=%s", type_name);
      return false;
    }

    JsonSkipWhitespace(&cursor);
    if (*cursor == ',') {
      cursor++;
      JsonSkipWhitespace(&cursor);
      continue;
    }
    if (*cursor == '}') {
      cursor++;
      JsonSkipWhitespace(&cursor);
      return (*cursor == '\0');
    }
    return false;
  }

  return false;
}

static bool PinmapValidateBindingsAgainstPinmap(const uint16_t* gpio_to_function) {
  if (!gpio_to_function) {
    return false;
  }

  bool helper_used[HX_PINMAP_MAX_GPIO] = {false};

  for (size_t i = 0; i < g_i2c_binding_count; i++) {
    const HxI2cDriverBinding* binding = &g_i2c_bindings[i];
    uint16_t sda_func = PinmapI2cSdaFunctionForPort(binding->port);
    uint16_t scl_func = PinmapI2cSclFunctionForPort(binding->port);
    if ((sda_func == HX_PIN_NONE) || (scl_func == HX_PIN_NONE)) {
      HX_LOGE("PIN", "i2c.%s.%u invalid port=%u", binding->type, (unsigned int)binding->instance, (unsigned int)binding->port);
      return false;
    }
    bool sda_found = false;
    bool scl_found = false;
    for (uint8_t gpio = 0; gpio < HX_TARGET_CAPS_CURRENT_DEF.gpio_count; gpio++) {
      if (gpio_to_function[gpio] == sda_func) {
        sda_found = true;
      }
      if (gpio_to_function[gpio] == scl_func) {
        scl_found = true;
      }
    }
    if (!sda_found || !scl_found) {
      HX_LOGE("PIN", "i2c.%s.%u port=%u missing SDA/SCL mapping", binding->type, (unsigned int)binding->instance, (unsigned int)binding->port);
      return false;
    }
  }

  for (size_t i = 0; i < g_uart_binding_count; i++) {
    const HxUartDriverBinding* binding = &g_uart_bindings[i];
    uint16_t tx_func = PinmapUartTxFunctionForPort((uint8_t)binding->uart_port);
    uint16_t rx_func = PinmapUartRxFunctionForPort((uint8_t)binding->uart_port);
    if ((tx_func == HX_PIN_NONE) || (rx_func == HX_PIN_NONE)) {
      HX_LOGE("PIN", "uart.%s.%u invalid uart=%d", binding->type, (unsigned int)binding->instance, (int)binding->uart_port);
      return false;
    }

    bool tx_found = false;
    bool rx_found = false;
    for (uint8_t gpio = 0; gpio < HX_TARGET_CAPS_CURRENT_DEF.gpio_count; gpio++) {
      if (gpio_to_function[gpio] == tx_func) {
        tx_found = true;
      }
      if (gpio_to_function[gpio] == rx_func) {
        rx_found = true;
      }
    }
    if (!tx_found || !rx_found) {
      HX_LOGE("PIN", "uart.%s.%u uart=%d missing TX/RX mapping", binding->type, (unsigned int)binding->instance, (int)binding->uart_port);
      return false;
    }

    int helper_values[3] = {binding->txen_gpio, binding->re_gpio, binding->de_gpio};
    const char* helper_names[3] = {"txen", "re", "de"};
    for (size_t idx = 0; idx < 3; idx++) {
      int gpio = helper_values[idx];
      if (gpio < 0) {
        continue;
      }
      if (gpio >= (int)HX_TARGET_CAPS_CURRENT_DEF.gpio_count) {
        HX_LOGE("PIN", "uart.%s.%u %s gpio=%d out of range", binding->type, (unsigned int)binding->instance, helper_names[idx], gpio);
        return false;
      }
      if (gpio_to_function[gpio] != HX_PIN_NONE) {
        HX_LOGE("PIN", "uart.%s.%u %s gpio=%d collides with %s", binding->type, (unsigned int)binding->instance, helper_names[idx], gpio, HxPinFunctionText(gpio_to_function[gpio]));
        return false;
      }
      if (helper_used[gpio]) {
        HX_LOGE("PIN", "uart.%s.%u %s gpio=%d already used by another helper", binding->type, (unsigned int)binding->instance, helper_names[idx], gpio);
        return false;
      }
      helper_used[gpio] = true;
    }
  }

  return true;
}

bool PinmapInit() {
  PinmapResetState();

  uint16_t next_gpio_to_function[HX_PINMAP_MAX_GPIO] = {0};
  uint8_t mapped_count = 0;
  if (!PinmapParseDenseArray(HxConfigData.board_pinmap, next_gpio_to_function, &mapped_count)) {
    HX_LOGE("PIN", "board.pinmap parse failed");
    return false;
  }

  if (!JsonValidateRootObject(HxConfigData.drivers_bindings)) {
    HX_LOGE("PIN", "drivers.bindings must be a valid JSON object");
    return false;
  }

  if (!PinmapBuildReverseLookup(next_gpio_to_function)) {
    return false;
  }

  if (!PinmapParseBindingsObject(HxConfigData.drivers_bindings)) {
    HX_LOGE("PIN", "drivers.bindings parse failed");
    return false;
  }

  if (!PinmapValidateBindingsAgainstPinmap(next_gpio_to_function)) {
    return false;
  }

  memcpy(g_gpio_to_function, next_gpio_to_function, sizeof(g_gpio_to_function));
  snprintf(g_bindings_json, sizeof(g_bindings_json), "%s", HxConfigData.drivers_bindings);
  g_mapped_count = mapped_count;

  g_pinmap_ready = true;
  Hx.pinmap_ready = true;
  HX_LOGI("PIN", "ready target=%s gpio_count=%u mapped=%u i2c=%u uart=%u bindings_len=%u",
          HX_TARGET_CAPS_CURRENT_DEF.name,
          (unsigned int)HX_TARGET_CAPS_CURRENT_DEF.gpio_count,
          (unsigned int)g_mapped_count,
          (unsigned int)g_i2c_binding_count,
          (unsigned int)g_uart_binding_count,
          (unsigned int)strlen(g_bindings_json));
  return true;
}

bool PinmapIsReady() {
  return g_pinmap_ready;
}

uint8_t PinmapGpioCount() {
  return HX_TARGET_CAPS_CURRENT_DEF.gpio_count;
}

uint16_t PinmapGetGpioCaps(uint8_t gpio) {
  return PinmapTargetCapFlags(gpio);
}

bool PinmapGetFunctionForGpio(uint8_t gpio, uint16_t* out_function) {
  if (!out_function || !g_pinmap_ready || (gpio >= HX_TARGET_CAPS_CURRENT_DEF.gpio_count)) {
    return false;
  }

  *out_function = g_gpio_to_function[gpio];
  return true;
}

int16_t PinmapGetGpioForFunction(uint16_t pin_function) {
  if (!g_pinmap_ready || (pin_function > HX_PINFUNC_MAX_ID)) {
    return -1;
  }
  return g_function_to_gpio[pin_function];
}

size_t PinmapMappedCount() {
  return g_mapped_count;
}

size_t PinmapI2cBindingCount() {
  return g_i2c_binding_count;
}

size_t PinmapUartBindingCount() {
  return g_uart_binding_count;
}

bool PinmapGetI2cBindingAt(size_t index, HxI2cDriverBinding* out_binding) {
  if (!g_pinmap_ready || !out_binding || (index >= g_i2c_binding_count)) {
    return false;
  }
  *out_binding = g_i2c_bindings[index];
  return true;
}

bool PinmapGetUartBindingAt(size_t index, HxUartDriverBinding* out_binding) {
  if (!g_pinmap_ready || !out_binding || (index >= g_uart_binding_count)) {
    return false;
  }
  *out_binding = g_uart_bindings[index];
  return true;
}

bool PinmapFindI2cBinding(const char* type, uint8_t instance, HxI2cDriverBinding* out_binding) {
  if (!g_pinmap_ready || !type || !type[0] || !out_binding) {
    return false;
  }
  for (size_t i = 0; i < g_i2c_binding_count; i++) {
    if ((g_i2c_bindings[i].instance == instance) && (strcmp(g_i2c_bindings[i].type, type) == 0)) {
      *out_binding = g_i2c_bindings[i];
      return true;
    }
  }
  return false;
}

bool PinmapFindUartBinding(const char* type, uint8_t instance, HxUartDriverBinding* out_binding) {
  if (!g_pinmap_ready || !type || !type[0] || !out_binding) {
    return false;
  }
  for (size_t i = 0; i < g_uart_binding_count; i++) {
    if ((g_uart_bindings[i].instance == instance) && (strcmp(g_uart_bindings[i].type, type) == 0)) {
      *out_binding = g_uart_bindings[i];
      return true;
    }
  }
  return false;
}

const char* PinmapBindingsJson() {
  return g_bindings_json;
}

bool PinmapCopyBindingsJson(char* out, size_t out_size) {
  if (!out || (out_size == 0)) {
    return false;
  }
  snprintf(out, out_size, "%s", g_bindings_json);
  return true;
}
