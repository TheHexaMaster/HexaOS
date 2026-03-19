# HexaOS States Manual

## Purpose

`state` is the persistent HexaOS layer used to track and store system, module, driver, and future script state.

Typical use cases:

- system counters and metadata, for example `sys.boot_count`
- stored driver and peripheral state
- counters, flags, and mutable runtime values
- custom persistent states created by the user or by a script

`state` is **not config**.  
`state` is intended for **mutable runtime data** that must survive reboot.

---

## Core Model

Each state has:

- `key` — a logical name, for example `sys.boot_count`, `drv.relay32.r01`
- `type` — `bool`, `int`, `string`
- optional limits:
  - for `int`: `min`, `max`
  - for `string`: `max_len`
- `owner` — state owner, for example `system`, `console`, later a module or script
- `flags` / visibility
- a **stored value**

Once created, a state:

- continues to exist after reboot
- has its definition restored from the persistent catalog
- has its value loaded from NVS

A state is therefore not just a RAM registration for the current boot.

---

## Supported State Types

HexaOS currently supports the following state types.

### 1. Boolean
Logical value:

- `true`
- `false`

Typical use:
- relay state
- enable/disable flag
- binary switches

---

### 2. Integer
32-bit signed integer (`int32`)

Typical use:
- counters
- indexes
- boot counters
- persistent numeric runtime data

Each integer state has a defined range:

- `min`
- `max`

---

### 3. String
Text value with a maximum length.

Typical use:
- names
- labels
- last text status
- identifiers

Each string state has a defined:

- `max_len`

---

## Origin Types

### Static state
A state defined at build time in `HX_STATE_SCHEMA(...)`.

Examples:

- `sys.boot_count`
- `sys.last_reset`

Properties:

- exists automatically as part of the system
- cannot be removed with `state delete`
- belongs to the static system schema

---

### Runtime state
A state created at runtime by:

- the console
- a module
- a driver
- later, a script

Examples:

- `usr.relay32.r01`
- `drv.energy.counter`
- `berry.script1.flag`

Properties:

- after creation, it is stored in the persistent catalog
- it remains available after reboot
- it can be removed with `state delete`

---

## Console Commands

## `state list`

Lists visible states.

### Syntax
```text
state list
state list <prefix>