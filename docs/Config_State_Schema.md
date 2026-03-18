# HexaOS Config and State Schema Manual

## Overview

HexaOS currently uses **two related but intentionally different schema systems**:

- **Config schema** for runtime configuration values stored in RAM and persisted to NVS as overrides.
- **State schema** for persistent runtime state stored directly in NVS.

Although both systems use schema macros and the shared `HxSchemaValueType` enum, they do **not** currently behave the same way.

This document describes the **actual behavior of the current codebase**.

---

## Shared Type Enum

Both systems use the same low-level value enum:

```cpp
enum HxSchemaValueType : uint8_t {
  HX_SCHEMA_VALUE_BOOL = 0,
  HX_SCHEMA_VALUE_INT32 = 1,
  HX_SCHEMA_VALUE_STRING = 2
};
```

This enum is the final runtime type identifier used by metadata tables and handlers.

---

# Config Schema

## Purpose

The config system is a **schema-driven runtime configuration model**.

It is designed for values that:

- have build-time defaults,
- live in a RAM structure,
- can be edited at runtime,
- can be reset to defaults,
- can be applied to the live system,
- and are persisted to NVS only when they differ from defaults.

Typical examples are:

- `device.name`
- `log.level`
- `safeboot.enable`

---

## Schema Definition

Config entries are declared with `HX_CONFIG_SCHEMA(X)`.

Current format:

```cpp
X(id, key_text, type_id, field_name, storage_size, max_len, min_i32, max_i32, default_value, console_visible, console_writable)
```

Meaning of each field:

- `id` - symbolic identifier used to generate constants and metadata.
- `key_text` - NVS key text such as `"device.name"`.
- `type_id` - symbolic type token: `STRING`, `INT32`, or `BOOL`.
- `field_name` - member name inside `struct HxConfig`.
- `storage_size` - storage size for string fields; unused for scalar fields.
- `max_len` - maximum allowed string length.
- `min_i32` - minimum allowed `INT32` value.
- `max_i32` - maximum allowed `INT32` value.
- `default_value` - build-time default value.
- `console_visible` - whether the key is shown in console listing.
- `console_writable` - whether the key can be changed through generic console config commands.

---

## What the Config Schema Generates

The config schema feeds multiple generated structures and constants.

### 1. `struct HxConfig`

The schema builds the RAM-backed configuration structure.

Each schema entry becomes a field inside:

```cpp
struct HxConfig
```

This is the authoritative in-memory config object.

---

### 2. `const HxConfig HxConfigDefaults`

The schema builds a complete default object:

```cpp
extern const HxConfig HxConfigDefaults;
```

This object contains the build-time defaults for all config fields.

---

### 3. `HxConfig HxConfigData`

This is the live mutable runtime config object:

```cpp
extern HxConfig HxConfigData;
```

It is the active RAM copy used by the system.

---

### 4. `HxConfigKeyDef`

Each config item is described by metadata:

```cpp
struct HxConfigKeyDef {
  const char* key;
  HxSchemaValueType type;
  size_t config_offset;
  size_t value_size;
  int32_t min_i32;
  int32_t max_i32;
  size_t max_len;
  bool console_visible;
  bool console_writable;
};
```

Important notes:

- `config_offset` points to the field inside `HxConfig`.
- `value_size` stores the field size in RAM.
- this metadata is enough to generically read, write, reset, and stringify config values.

---

### 5. `kHxConfigKeys[]`

The schema feeds a static metadata table:

```cpp
static const HxConfigKeyDef kHxConfigKeys[]
```

This table is used for generic lookup and iteration.

---

### 6. Per-key text constants

For every config entry, the schema generates a string constant such as:

```cpp
HX_CFG_DEVICE_NAME
HX_CFG_LOG_LEVEL
HX_CFG_SAFEBOOT_ENABLE
```

These constants hold the NVS key text.

---

## Config Runtime Model

Config is **RAM-backed**.

The main flow is:

1. `ConfigInit()` resets `HxConfigData` to defaults and opens the config NVS namespace.
2. `ConfigLoad()` resets `HxConfigData` to defaults again and then reads NVS overrides.
3. `ConfigApply()` pushes selected values from `HxConfigData` into live runtime state.
4. `ConfigSave()` stores only values that differ from `HxConfigDefaults`.

This means:

- RAM holds the full current configuration.
- NVS stores only deviations from defaults.

---

## Config Validation Behavior

Config validation is schema-driven.

The handler uses the metadata table to enforce:

- type correctness,
- string maximum length,
- integer range limits,
- console writability,
- and field size correctness.

Examples:

- strings longer than `max_len` are rejected,
- integers outside `min_i32` / `max_i32` are rejected,
- non-writable keys cannot be changed through generic config parsing.

---

## Config Console Workflow

The console supports a full config workflow:

- `listcfg`
- `readcfg <key>`
- `setcfg <key> <value>`
- `savecfg`
- `loadcfg`
- `defaultcfg`

Important behavior:

- `setcfg` updates `HxConfigData` in RAM.
- `savecfg` persists overrides to NVS.
- `loadcfg` reloads from NVS on top of defaults.
- `defaultcfg` resets config values back to defaults.

This makes config suitable for editable user-facing or system-facing settings.

---

# State Schema

## Purpose

The state system is a **persistent runtime state registry**, not a full RAM-backed config model.

It is intended for values that:

- represent operational state,
- must survive reboot,
- are usually stored immediately,
- are not handled as configuration defaults,
- and are not exposed through a full editable config workflow.

Typical examples are:

- `sys.boot_count`
- `sys.last_reset`

---

## Schema Definition

State entries are declared with `HX_STATE_SCHEMA(X)`.

Current format:

```cpp
X(id, key_text, type_id, min_i32, max_i32, max_len, console_visible)
```

Meaning of each field:

- `id` - symbolic identifier used to generate constants and metadata.
- `key_text` - NVS key text such as `"sys.boot_count"`.
- `type_id` - **direct low-level enum value**, for example `HX_SCHEMA_VALUE_INT32` or `HX_SCHEMA_VALUE_STRING`.
- `min_i32` - minimum allowed integer value stored in metadata.
- `max_i32` - maximum allowed integer value stored in metadata.
- `max_len` - maximum string length stored in metadata.
- `console_visible` - whether the key is shown in state console listing.

Important difference from config:

The state schema currently does **not** include:

- `field_name`
- `storage_size`
- `default_value`
- `console_writable`

It also does **not** use the symbolic config-style type tokens `STRING`, `INT32`, and `BOOL`.
Instead, it uses the final enum values directly.

---

## What the State Schema Generates

The current state schema generates much less infrastructure than config.

### 1. `HxStateKeyDef`

State metadata is described by:

```cpp
struct HxStateKeyDef {
  const char* key;
  HxSchemaValueType type;
  int32_t min_i32;
  int32_t max_i32;
  size_t max_len;
  bool console_visible;
};
```

This metadata is descriptive only.
It does not contain a RAM offset because state is not backed by a `struct HxState` object.

---

### 2. `kHxStateKeys[]`

The schema feeds a static metadata table:

```cpp
static const HxStateKeyDef kHxStateKeys[]
```

This table is used for lookup, iteration, and console display.

---

### 3. Per-key text constants

For every state entry, the schema generates a string constant such as:

```cpp
HX_STATE_BOOT_COUNT
HX_STATE_LAST_RESET
```

These constants hold the NVS key text.

---

## What the State Schema Does **Not** Generate

The current codebase does **not** generate or provide any of the following:

- `struct HxState`
- `HxStateDefaults`
- `HxStateData`

This is the most important architectural difference compared with config.

---

## State Runtime Model

State is **not RAM-backed as a unified struct**.

Instead, the current implementation behaves like this:

- `StateInit()` opens the state NVS namespace.
- `StateLoad()` does not load a full RAM object.
- `StateGetBool()` and `StateGetInt()` read directly from NVS.
- `StateSetBool()` and `StateSetInt()` write directly to NVS and immediately commit.
- `StateValueToString()` reads the value directly from NVS on demand.

This means state is currently a **direct NVS-backed key-value layer** with schema metadata on the side.

---

## State Validation Behavior

This is a critical difference.

Although the state schema contains metadata such as:

- type,
- integer limits,
- string max length,

that metadata is **not currently used as the authoritative validation layer** for the public state API.

The public API is currently:

```cpp
bool StateGetBool(const char* key, bool defval);
int32_t StateGetInt(const char* key, int32_t defval);
bool StateSetBool(const char* key, bool value);
bool StateSetInt(const char* key, int32_t value);
```

These functions:

- accept an arbitrary key string,
- talk directly to NVS,
- do not require the key to exist in `HX_STATE_SCHEMA`,
- do not enforce schema range rules,
- do not use a RAM state struct,
- and do not provide a generic string setter.

Therefore, state currently behaves as a **raw persistent key-value API plus metadata**, not as a fully enforced schema-driven model.

---

## State Load Side Effect

`StateLoad()` has one built-in side effect:

- it reads `sys.boot_count`,
- increments it,
- stores it back immediately.

So state load is not just a passive load step.
It also updates persistent operational state.

---

## State Console Workflow

The console currently supports:

- `liststate`
- `readstate <key>`

There is no generic console workflow matching config.

There is currently no built-in equivalent of:

- `setstate`
- `savestate`
- `loadstate`
- `defaultstate`

This matches the current design: state is for internal persistent runtime values, not for full interactive config-style editing.

---

# Direct Comparison

## Config

Config is:

- schema-driven,
- RAM-backed,
- default-backed,
- resettable,
- apply-capable,
- validated through schema metadata,
- stored to NVS as overrides only.

## State

State is:

- metadata-described,
- directly NVS-backed,
- not backed by a `struct HxState`,
- not backed by defaults,
- not reset through a generic default model,
- not fully validated through schema metadata in the public API,
- committed immediately on setter calls.

---

# Summary

In the current HexaOS implementation:

- **Config** is a complete schema-driven configuration subsystem.
- **State** is a lighter persistent runtime state subsystem with schema metadata, but without the same RAM-backed architecture or enforcement model.

The two systems share naming patterns and type concepts, but they are **not currently symmetrical by design**.

---

# Practical Guidance

Use **config** for values that need:

- defaults,
- structured RAM storage,
- generic editable behavior,
- validation,
- and runtime apply logic.

Use **state** for values that need:

- persistence across reboot,
- direct immediate storage,
- simple lookup by key,
- and lightweight metadata for browsing or introspection.

