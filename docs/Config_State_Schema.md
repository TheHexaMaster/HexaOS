# HexaOS Config / State Schema Manual

## Overview

HexaOS currently uses two related but intentionally different persistent data systems:

- **Config**: runtime configuration values that are loaded into RAM, can be edited, validated, applied, and saved back to Non-Volatile Storage (NVS).
- **State**: persistent runtime state values that represent system status across reboots. These values are also described by a schema, but their operational model is not identical to config.

Even though both systems use schema macros and NVS, they are **not the same abstraction**.

---

## 1. Config system

### Purpose

The config system is designed for **user-editable or system-editable configuration**.
Typical examples are:

- device name
- log level
- safeboot enable flag

These values:

- exist as strongly typed fields in RAM
- have compile-time defaults
- can be listed and edited through the console
- can be validated before acceptance
- can be applied to the running system
- are persisted as NVS overrides

### Source of truth

The config system is defined by `HX_CONFIG_SCHEMA(X)`.

Each config entry contains enough metadata to drive the whole config pipeline:

- symbolic identifier
- text key stored in NVS
- schema type (`STRING`, `INT32`, `BOOL`)
- target field name inside `HxConfig`
- storage size
- maximum string length
- minimum and maximum `int32_t` range
- default value
- console visibility flag
- console writability flag

### Runtime model

Config values are backed by two RAM objects:

- `HxConfigData` -> current active runtime config
- `HxConfigDefaults` -> compile-time defaults generated from the schema

The schema is used to build:

- the `HxConfig` struct
- the defaults object
- the key definition table `kHxConfigKeys[]`
- generic read/write/parse/format helpers

### Storage strategy

Config follows an **override model**:

1. Defaults are loaded into RAM.
2. NVS values override defaults when present.
3. Saving writes only values that differ from defaults.
4. If a value matches its default, the stored NVS override is removed.

This means NVS stores only the delta from build defaults, not a full copy of the config object.

### Validation

Config supports centralized validation through schema metadata:

- boolean text parsing
- integer parsing with min/max range enforcement
- string length enforcement
- console visibility and writability control

### Console workflow

Config is a full console-editable subsystem.

Supported commands:

- `listcfg`
- `readcfg <key>`
- `setcfg <key> <value>`
- `savecfg`
- `loadcfg`
- `defaultcfg`

Important behavior:

- `setcfg` updates RAM first
- `ConfigApply()` applies runtime effects
- persistence happens through `savecfg`

This gives config a staged workflow: **edit -> test/apply -> save**.

---

## 2. State system

### Purpose

The state system is designed for **persistent runtime state**, not for normal configuration editing.
Typical examples are:

- boot counter
- last reset reason string

State exists to keep track of values that are part of system operation rather than user configuration.

### Source of truth

The state system is defined by `HX_STATE_SCHEMA(X)`.

In the current codebase, the state schema uses the same style as config entries and currently includes:

- symbolic identifier
- text key stored in NVS
- schema type (`STRING`, `INT32`, `BOOL`)
- target field name inside `HxState`
- storage size
- maximum string length
- minimum and maximum `int32_t` range
- default value
- console visibility flag
- console writability flag

### Runtime model

State values are backed by two RAM objects:

- `HxStateData` -> current runtime state
- `HxStateDefaults` -> schema-generated default state

The schema is used to build:

- the `HxState` struct
- the defaults object
- the key definition table `kHxStateKeys[]`
- generic formatting and parsing helpers

In addition, the state handler tracks whether a schema-defined key is actually present in NVS through an internal presence table.

### Storage strategy

State behaves as a **persistent runtime record** rather than a configuration overlay.

Key characteristics:

- state values are loaded from NVS into RAM
- each key can be tracked as present or absent in storage
- state is meant to reflect operational values
- state writes are generally immediate persistence operations

This is different from config's edit-and-save workflow.

### Validation

State supports schema-based type validation for:

- boolean parsing
- integer parsing with min/max range enforcement
- string max length enforcement

However, state is still conceptually different from config because it represents runtime information rather than operator-controlled settings.

### Console workflow

State is currently exposed primarily as a read-oriented console subsystem.

Supported commands:

- `liststate`
- `readstate <key>`

There is no equivalent public operator workflow matching config commands such as:

- staged edit in RAM
- explicit save after editing
- full interactive state editing pipeline

This is intentional: state is treated as system runtime data first.

---

## 3. Schema field meaning

Both schemas currently use the same field order:

```cpp
X(
  ID,
  "text.key",
  TYPE_ID,
  field_name,
  storage_size,
  max_len,
  min_i32,
  max_i32,
  default_value,
  console_visible,
  console_writable
)
```

### Field description

#### `ID`
Compile-time symbolic name used for internal declarations and generated constants.

Example:

```cpp
DEVICE_NAME
BOOT_COUNT
```

#### `"text.key"`
Persistent text key used in NVS and console commands.

Example:

```cpp
"device.name"
"sys.boot_count"
```

#### `TYPE_ID`
Schema type token:

- `STRING`
- `INT32`
- `BOOL`

These tokens are later mapped internally to `HxSchemaValueType`.

#### `field_name`
Name of the field in the generated RAM struct (`HxConfig` or `HxState`).

Example:

```cpp
log_level
boot_count
```

#### `storage_size`
Allocated storage size in bytes for the field when applicable.
For strings this includes room for the terminating null character.
For scalar fields it is usually informational because `sizeof(field)` is used by the key table.

#### `max_len`
Maximum allowed string payload length excluding the null terminator.
Used only for `STRING` entries.

#### `min_i32`, `max_i32`
Allowed numeric range for `INT32` entries.
Ignored for other types.

#### `default_value`
Compile-time default written into the generated defaults object.

Examples:

```cpp
HX_BUILD_DEFAULT_DEVICE_NAME
(int32_t)HX_BUILD_DEFAULT_LOG_LEVEL
0
""
```

#### `console_visible`
Controls whether the entry should appear in schema-driven console listing/output.

#### `console_writable`
Controls whether the entry is intended to be writable through schema-driven console workflows.
In practice this matters much more for config than for state.

---

## 4. Generated objects and metadata

### Config side

The config schema generates:

- `struct HxConfig`
- `const HxConfig HxConfigDefaults`
- `HxConfig HxConfigData`
- `HxConfigKeyDef`
- `kHxConfigKeys[]`
- per-key text constants such as `HX_CFG_DEVICE_NAME`

### State side

The state schema generates:

- `struct HxState`
- `const HxState HxStateDefaults`
- `HxState HxStateData`
- `HxStateKeyDef`
- `kHxStateKeys[]`
- per-key text constants such as `HX_STATE_BOOT_COUNT`

---

## 5. Lifecycle difference

### Config lifecycle

Typical config flow:

1. `ConfigInit()` initializes the subsystem.
2. Defaults are prepared in RAM.
3. `ConfigLoad()` overlays stored NVS values onto RAM.
4. `ConfigApply()` applies active config to runtime services.
5. Console edits modify RAM.
6. `ConfigSave()` persists non-default overrides.

This is a classic **configuration management workflow**.

### State lifecycle

Typical state flow:

1. `StateInit()` initializes the subsystem.
2. `StateLoad()` loads known schema state from NVS into RAM.
3. Runtime logic updates state values as needed.
4. State is persisted as operational data.

State is therefore a **runtime continuity workflow**, not a full configuration management workflow.

---

## 6. Practical behavioral difference

### Config behavior in one sentence

Config is a **schema-driven editable runtime configuration model with defaults, validation, apply logic, and selective NVS override storage**.

### State behavior in one sentence

State is a **schema-described persistent runtime data model focused on preserving operational values across reboots, with a more conservative and read-oriented external workflow**.

---

## 7. Current schema examples

### Config example

```cpp
#define HX_CONFIG_SCHEMA(X) \
  X(DEVICE_NAME,        "device.name",        STRING, device_name,      33, 32, 0,                      0,                      HX_BUILD_DEFAULT_DEVICE_NAME,            true, true) \
  X(LOG_LEVEL,          "log.level",          INT32,  log_level,         0,  0, (int32_t)HX_LOG_ERROR, (int32_t)HX_LOG_DEBUG, (int32_t)HX_BUILD_DEFAULT_LOG_LEVEL, true, true) \
  X(SAFEBOOT_ENABLE,    "safeboot.enable",    BOOL,   safeboot_enable,   0,  0, 0,                      1,                      (HX_BUILD_DEFAULT_SAFEBOOT_ENABLE != 0), true, true)
```

### State example

```cpp
#define HX_STATE_SCHEMA(X) \
  X(BOOT_COUNT,         "sys.boot_count",     INT32,  boot_count,        0,  0, 0,         INT32_MAX, 0,  true, false) \
  X(LAST_RESET,         "sys.last_reset",     STRING, last_reset,       33, 32, 0,         0,         "", true, false)
```

---

## 8. Design guidance

Use **config** when a value:

- has a meaningful default
- should be editable by the operator or firmware logic
- needs validation and range enforcement
- may require runtime apply logic
- belongs to device setup rather than runtime history

Use **state** when a value:

- represents runtime history or operational continuity
- should survive reboot as part of system status
- is not part of ordinary user configuration
- is mainly observed rather than interactively managed

---

## 9. Summary

HexaOS uses two schema-backed persistent systems with different intent:

- **Config** is for editable configuration management.
- **State** is for persistent operational state.

They share a common schema style and common type system, but they are used differently in the runtime architecture.

That distinction is important and should remain clear when extending the platform.
