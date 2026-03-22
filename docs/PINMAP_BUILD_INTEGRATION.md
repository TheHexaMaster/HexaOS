# HexaOS Pin Definition Manual (v2)

## Purpose

HexaOS pin definition is split into two separate config defaults generated at build time:

- `board.pinmap`
- `drivers.bindings`

The goal is to keep **physical board routing** separate from **driver instance binding**.

---

## 1. `board.pinmap`

`board.pinmap` describes only physical GPIO routing.

- Format: dense JSON array
- Array index: physical GPIO number
- Array value: `HX_PIN_*` logical pin function id

Example:

```json
[0,0,0,0,0,0,0,200,201,0,0,0,0,0,802,803]
```

Meaning:

- GPIO 7 = `HX_PIN_I2C0_SDA`
- GPIO 8 = `HX_PIN_I2C0_SCL`
- GPIO 14 = `HX_PIN_HOSTED0_D0`
- GPIO 15 = `HX_PIN_HOSTED0_D1`

`board.pinmap` never describes driver instances or protocols.

---

## 2. `drivers.bindings`

`drivers.bindings` describes active build-time driver instances and the native interface they use.

### Correct hierarchy

Top-level keys are always **driver family names**.

Each instance object contains:

- one native interface selector such as `uart`, `i2c`, `spi`
- optional driver-specific parameters

### Correct example

```json
{
  "RS485": {
    "0": {
      "uart": 2,
      "txen": 17,
      "re": -1,
      "de": -1
    }
  },
  "DS3232": {
    "0": {
      "i2c": 0,
      "address": 104
    }
  },
  "HDC2010": {
    "0": {
      "i2c": 1,
      "address": 65
    }
  }
}
```

### Why this is correct

Because:

- `UART`, `I2C`, `SPI` are native SoC peripherals
- `RS485`, `DS3232`, `HDC2010` are drivers
- `MODBUS` is a higher protocol and does not belong into this layer

So:

- `RS485` is **not** a native bus family section
- `RS485` is a driver family name
- `uart` is its native transport selector

---

## 3. Build model

### 3.1 Native pin defaults

The active board/variant selects the real `pins_arduino.h` outside `src`.

That file provides default aliases such as:

- `TX`
- `RX`
- `SDA`
- `SCL`
- `MOSI`
- `MISO`
- `SCK`
- board-specific Ethernet or Hosted SDIO pins

HexaOS imports those defaults and normalizes them into `board.pinmap`.

### 3.2 Build overrides

`headers/hx_build.h` can override imported pin defaults using:

```c
#define HX_BUILD_PIN_OVERRIDE_LIST(X) \
  X(HX_PIN_UART0_TX, 12) \
  X(HX_PIN_UART0_RX, 13)
```

Priority is:

`hx_build.h override > imported pins_arduino default`

### 3.3 Driver registries

Driver family names are declared by native interface family.

#### I2C driver registry

```c
#define HX_BUILD_I2C_DRIVER_TYPE_LIST(X) \
  X(DS3232) \
  X(HDC2010)
```

#### UART driver registry

```c
#define HX_BUILD_UART_DRIVER_TYPE_LIST(X) \
  X(RS485)
```

---

## 4. Driver instance macros

### I2C examples

```c
#define HX_I2C_DRIVER_DS3232_0_ENABLED   1
#define HX_I2C_DRIVER_DS3232_0_PORT      0
#define HX_I2C_DRIVER_DS3232_0_ADDRESS   0x68

#define HX_I2C_DRIVER_HDC2010_0_ENABLED  1
#define HX_I2C_DRIVER_HDC2010_0_PORT     1
#define HX_I2C_DRIVER_HDC2010_0_ADDRESS  0x41
```

Generated JSON:

```json
{
  "DS3232": {
    "0": {
      "i2c": 0,
      "address": 104
    }
  },
  "HDC2010": {
    "0": {
      "i2c": 1,
      "address": 65
    }
  }
}
```

### UART driver example: RS485

```c
#define HX_BUILD_UART_DRIVER_TYPE_LIST(X) \
  X(RS485)

#define HX_UART_DRIVER_RS485_0_ENABLED    1
#define HX_UART_DRIVER_RS485_0_PORT       2
#define HX_UART_DRIVER_RS485_0_TXEN_GPIO  17
#define HX_UART_DRIVER_RS485_0_RE_GPIO    -1
#define HX_UART_DRIVER_RS485_0_DE_GPIO    -1
```

Generated JSON:

```json
{
  "RS485": {
    "0": {
      "uart": 2,
      "txen": 17,
      "re": -1,
      "de": -1
    }
  }
}
```

---

## 5. Build generator

The prebuild script:

- reads `pins_arduino.h`
- reads `hx_build.h`
- builds `board.pinmap`
- builds `drivers.bindings`
- writes `headers/hx_build_layout_autogen.h`

Output macros:

```c
#define HX_BUILD_DEFAULT_BOARD_PINMAP_JSON "..."
#define HX_BUILD_DEFAULT_DRIVERS_BINDINGS_JSON "..."
```

These become default flash-backed config values in `hx_config.h`.

---

## 6. Boot flow

At boot:

1. config defaults are loaded from flash
2. NVS overrides are applied if present
3. `board.pinmap` is parsed and validated
4. `drivers.bindings` is parsed and validated
5. runtime lookup tables are created

If the layout is invalid, boot fails early and clearly.

---

## 7. Runtime API

### GPIO mapping

- `PinmapGetFunctionForGpio()`
- `PinmapGetGpioForFunction()`
- `PinmapMappedCount()`

### I2C bindings

- `PinmapI2cBindingCount()`
- `PinmapGetI2cBindingAt()`
- `PinmapFindI2cBinding()`

### UART bindings

- `PinmapUartBindingCount()`
- `PinmapGetUartBindingAt()`
- `PinmapFindUartBinding()`

---

## 8. Validation rules

### `board.pinmap`

Validated against target capability database:

- GPIO range
- reserved flash pins
- reserved PSRAM pins
- input-only conflicts
- duplicate logical function assignment
- strap pin warnings

### `drivers.bindings`

Validated semantically:

- I2C driver binding must reference a real SDA/SCL mapping for that port
- UART driver binding must reference a real TX/RX mapping for that port
- helper GPIO such as `txen`, `re`, `de` must not collide with mapped board pins
- helper GPIO must not collide with each other

---

## 9. Design rules for the future

1. `board.pinmap` describes only physical routing.
2. `drivers.bindings` describes only driver instances and native interface binding.
3. Top-level JSON keys in `drivers.bindings` are always driver names.
4. Native interface type is stored inside each instance object.
5. Protocol layers such as `MODBUS` do not belong into this definition layer.
6. Instance numbering always starts at `0`.
7. Imported `pins_arduino.h` defaults are normal inputs, not a competing HexaOS board system.

