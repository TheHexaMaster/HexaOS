# HexaOS Architecture

## Purpose

This document describes the **current source architecture** of HexaOS based on the reviewed `src1617` archive and records the **target direction** that the tree is moving toward.

It is not a frozen doctrine. It should track the real codebase and be updated whenever the implementation materially changes.

HexaOS should remain:

- explicit,
- compact in concept,
- easy to audit,
- easy to extend,
- resistant to layer mixing,
- honest about what is current versus what is only planned.

---

## Core Principle

HexaOS is not built around heavy class hierarchies or framework-style abstraction.

It is built around **clear ownership**:

- **Core** holds the permanent system skeleton and execution bridge.
- **Adapters** bridge HexaOS to external APIs, SDK layers, framework backends, or low-level platform resources.
- **Handlers** own internal HexaOS domains and their policy.
- **Modules** are cooperative lifecycle participants registered into the runtime dispatcher.
- **Commands** provide frontend-agnostic command dispatch.
- **Drivers** are reserved for reusable device/protocol implementations above adapters.
- **Services** are reserved for composed hosted/client runtime functions above handlers, drivers, and adapters.

---

## Important Clarification

HexaOS must distinguish between:

1. **core**
2. **mandatory for a given product build**
3. **optional runtime feature**

These are not the same thing.

A component may be mandatory for a given product profile without belonging to Core.

**Core must remain small.**

---

## Current Source Tree (reviewed archive)

```text
src/
  headers/
    hx_build.h
    hx_config.h

  hexaos.cpp

  system/
    adapters/
      console_adapter.*
      nvs_adapter.*
      rtos_adapter.*

    commands/
      command_builtin.*
      command_engine.*

    core/
      log.*
      module_registry.*
      panic.*
      rtos.*
      runtime.*
      user_interface.*

    drivers/
      (currently empty)

    handlers/
      littlefs_handler.*
      nvs_config_handler.*
      nvs_state_handler.*
      user_interface_handler.*

    modules/
      mod_berry.cpp
      mod_lvgl.cpp
      mod_storage.cpp
      mod_system.cpp
      mod_web.cpp
```

Notes about the current tree:

- There is **no `hexaos.h`** in the reviewed archive.
- There is **no `system/core/boot.*`** yet. Boot orchestration currently lives inside `hexaos.cpp` as file-local helpers.
- There is **no `mod_console.*`** anymore. Interactive shell control has already been split into `core/user_interface.*` and `handlers/user_interface_handler.*`.
- There is **no `headers/include/`** subtree in the reviewed archive.

---

## Current Runtime Flow

The current runtime flow in the reviewed archive is:

### `setup()`

- RTOS init flag setup
- Log init
- User interface init
- Boot init sequence

### `BootInit()` inside `hexaos.cpp`

- boot banner
- reset reporting
- chip reporting
- config init
- config load
- config apply
- state init
- files init
- state load
- files mount
- command init
- user interface start
- module init all
- module start all

### `loop()`

- uptime update
- `UserInterfaceLoop()`
- `ModuleLoopAll()`
- `StateLoop()`
- `ModuleEvery100ms()`
- `ModuleEverySecond()`

This means the current system is **not purely module-driven**.

Two important runtime parts already bypass the module layer by design:

- the local control plane (`UserInterfaceLoop()`),
- the delayed state-commit scheduler (`StateLoop()`).

That is acceptable in the current architecture because both are acting as mandatory runtime services rather than optional feature modules.

---

## Current Layer Definitions

### 1. Core

`system/core/`

Core currently contains the permanent system skeleton.

Current real Core responsibilities:

- logging,
- runtime flags/state container,
- panic path,
- RTOS wrapper,
- module registry and lifecycle fan-out,
- mandatory local control-plane bridge.

Important current note:

- Boot orchestration is **conceptually core-like**, but it is **not yet in `system/core/boot.*`**. It still lives in `hexaos.cpp`.

**Core is the system skeleton, not the feature layer.**

---

### 2. Adapter

`system/adapters/`

An adapter is the boundary to something external.

Current real adapters:

- `console_adapter.*` -> local console transport backend abstraction
- `nvs_adapter.*` -> ESP NVS backend abstraction
- `rtos_adapter.*` -> FreeRTOS backend abstraction

Adapter rule:

- an adapter should bridge one external/backend/resource boundary,
- it should not become the owner of HexaOS domain policy.

Current interpretation note:

- `console_adapter.*` is acceptable in the current tree because it abstracts the active **local control transport backend**.
- If UART later becomes a broader shared platform resource, the architecture should split that into a generic `uart_adapter.*` plus a higher-level consumer.

---

### 3. Handler

`system/handlers/`

A handler owns an internal HexaOS domain.

Current real handlers:

- `nvs_config_handler.*` -> configuration domain
- `nvs_state_handler.*` -> runtime state persistence domain
- `littlefs_handler.*` -> filesystem domain on top of LittleFS
- `user_interface_handler.*` -> shell/session/input domain

Handler rule:

- a handler owns validation, state, policy, and stable internal API for a HexaOS domain,
- it is not merely a thin SDK wrapper.

---

### 4. Module

`system/modules/`

A module is a cooperative lifecycle participant.

Current modules:

- `mod_system.cpp`
- `mod_storage.cpp`
- `mod_web.cpp`
- `mod_lvgl.cpp`
- `mod_berry.cpp`

Current module reality:

- `mod_system` is the only meaningful active runtime example today,
- the remaining modules are mostly lifecycle shells/placeholders,
- modules are currently compiled from `.cpp` only; there are no dedicated module headers yet.

Module rule:

- a module moves the system in time,
- it should not automatically become the owner of a domain just because it has periodic hooks.

---

### 5. Commands

`system/commands/`

Commands provide a frontend-agnostic command surface.

Current command layer responsibilities:

- command registry,
- command lookup,
- line execution,
- help/inspection/mutation commands for config/state/log/files/runtime.

Command rule:

- commands trigger internal APIs,
- commands should not become the owner of domain state or persistence rules.

---

### 6. Drivers

`system/drivers/`

The directory already exists but is not yet populated in the reviewed archive.

Planned meaning:

- reusable device/protocol implementations above adapters.

Examples for the future:

- I2C device drivers,
- RTC drivers,
- EEPROM/FRAM drivers,
- IO expander drivers,
- display or sensor protocol drivers.

---

### 7. Services

`system/services/` (planned, not present yet)

A service should represent a composed runtime function above handlers/drivers/adapters.

Examples for the future:

- web service,
- hosted API service,
- async HTTP client service,
- telnet/web console service.

A service is not automatically a handler.

---

## Current Transitional Reality

### User interaction is already no longer a module

The reviewed code has already moved away from `mod_console`.

Current split:

- `core/user_interface.*` -> mandatory runtime bridge and transport polling
- `handlers/user_interface_handler.*` -> shell/session/prompt/command handling
- `adapters/console_adapter.*` -> backend transport boundary

That is a meaningful improvement and should now be reflected in the architecture text instead of still describing `mod_console` as the current implementation.

### Boot is still entrypoint-local

The current boot sequence is conceptually core functionality, but physically it still lives in `hexaos.cpp` as static helpers.

That is acceptable for now, but the document should treat it as **current reality plus future migration target**, not as if `core/boot.*` already exists.

### Module layer is still early

`mod_storage`, `mod_web`, `mod_lvgl`, and `mod_berry` are still placeholder lifecycle shells.

That means the module layer is still structurally open and can be redesigned further without breaking a mature subsystem model.

---

## Agreed Direction: User Interface

HexaOS should continue using the current split that now exists in the codebase.

### Current good direction

#### Core bridge

`core/user_interface.*`

Should remain:

- small,
- mandatory,
- transport/polling oriented,
- free of shell policy.

#### Domain owner

`handlers/user_interface_handler.*`

Should remain the owner of:

- prompt state,
- line editing,
- session state,
- redraw policy,
- command dispatch bridge,
- future multi-endpoint shell/session policy.

#### Transport/backend boundary

`adapters/console_adapter.*`

Currently owns the selected local console backend.

Future evolution may split this further if HexaOS introduces a broader shared UART/USB transport layer, but that is not required yet.

#### Remote control endpoints

Future remote or hosted control channels should become **services** that feed the same `user_interface_handler` domain rather than owning separate shell logic.

Examples:

- `web_console_service.*`
- `telnet_console_service.*`

---

## Agreed Direction: Networking

Networking should eventually be split into three concepts.

### 1. Uplink backends

Adapters such as:

- `wifi_adapter.*`
- `ethernet_adapter.*`
- optionally `lwip_adapter.*`

### 2. Network domain owner

Handler such as:

- `network_handler.*`

This should own:

- uplink state,
- readiness,
- addressing,
- reconnect/fallback policy,
- uniform API for the rest of HexaOS.

### 3. Services above network

Modules + services pattern, for example:

- `mod_network.*`
- `mod_services.*`
- `web_service.*`
- `async_http_client_service.*`
- `mqtt_service.*`

Important design point:

**Web is not the network domain itself.**
It is a service running above networking.

---

## Recommended Future Tree

```text
src/
  headers/
    hx_build.h
    hx_config.h
    hx_state.h
    hx_files.h
    hx_network.h
    hx_user_interface.h

  hexaos.cpp

  system/
    core/
      boot.*
      log.*
      panic.*
      rtos.*
      runtime.*
      module_registry.*
      user_interface.*

    adapters/
      console_adapter.*
      nvs_adapter.*
      rtos_adapter.*
      uart_adapter.*
      wifi_adapter.*
      ethernet_adapter.*
      lwip_adapter.*

    handlers/
      littlefs_handler.*
      nvs_config_handler.*
      nvs_state_handler.*
      user_interface_handler.*
      uart_handler.*
      network_handler.*
      files_handler.*

    drivers/
      rtc/
        ds3231_driver.*
      eeprom/
        at24c32_driver.*
      ioexpander/
        xl9535_driver.*

    services/
      web_service.*
      web_console_service.*
      telnet_console_service.*
      async_http_client_service.*

    commands/
      command_engine.*
      command_builtin.*

    modules/
      mod_system.cpp
      mod_network.cpp
      mod_services.cpp
      mod_storage.cpp
      mod_lvgl.cpp
      mod_berry.cpp
```

This is the **target direction**, not a claim that the current tree already implements all of it.

---

## Dependency Direction

### Allowed

- Core may depend on shared headers and core-internal helpers.
- Adapters may depend on external APIs and internal shared headers.
- Handlers may depend on adapters, core, and shared headers.
- Drivers may depend on adapters and shared headers.
- Services may depend on handlers, drivers, adapters, and shared headers.
- Modules may depend on handlers, services, core, and shared headers.
- Commands may depend on handlers, services, core, and shared headers.

### Discouraged

- Core depending on optional feature modules.
- Adapters containing domain policy.
- Handlers becoming raw SDK wrappers.
- Consumer-specific adapters duplicating a generic backend adapter after a shared backend owner already exists.
- Modules owning multiple unrelated layers at once.
- Commands owning domain state.

---

## Naming Rules

### Use `*_adapter` when the file:

- bridges HexaOS to an external API, SDK, framework, or hardware backend,
- represents one external/backend boundary once.

### Use `*_handler` when the file:

- owns a HexaOS domain,
- contains domain policy,
- manages state or stable internal API.

### Use `*_driver` when the file:

- implements a reusable device/protocol driver above adapters.

### Use `*_service` when the file:

- composes behavior across handlers/drivers/adapters,
- provides hosted or client runtime service functionality,
- is not itself a domain owner.

### Use `mod_*` when the file:

- participates in cooperative lifecycle scheduling.

### Use `hx_*` when the file:

- defines public or shared HexaOS-facing headers.

---

## Architectural Anti-Patterns

Avoid these patterns:

### 1. Layer hybrids

One file should not simultaneously be:

- adapter,
- handler,
- service,
- scheduler,
- command surface.

### 2. Duplicate backend ownership

Do not create separate adapters for different consumers of the same backend after a generic backend boundary already exists.

### 3. Mandatory == Core

Do not move a feature into Core only because a particular product build requires it.

### 4. Service logic hidden inside adapters

An adapter must not become the place where reconnect policy, session logic, application routing, or business behavior accumulate.

### 5. Domain logic inside commands

Commands should trigger domain APIs, not become the domain.

---

## Current Working Interpretation

As of the reviewed archive:

- the Core skeleton is meaningful and usable,
- config and state are strong handlers,
- LittleFS is functionally handler-like and appropriately placed,
- user interaction is already split across core bridge + handler + adapter,
- the module layer is still transitional,
- `drivers/` exists but is not populated yet,
- `services/` is still a planned layer,
- boot orchestration still lives in `hexaos.cpp` and is a likely future extraction target.

---

## Final Summary

HexaOS should currently be understood with this model:

- **Core** = permanent system skeleton and mandatory execution bridge
- **Adapter** = external/backend boundary
- **Handler** = owner of a HexaOS domain
- **Driver** = reusable device/protocol implementation above adapters
- **Service** = composed runtime function above handlers/drivers/adapters
- **Module** = lifecycle participant
- **Commands** = frontend-agnostic command surface

Additional agreed direction:

- the user/control plane should continue evolving around `user_interface` rather than a legacy `console module` identity,
- shared backends such as UART should be owned once if and when they become broader shared platform resources,
- networking should be treated as a domain distinct from the services running above it,
- the architecture document must follow the real codebase and explicitly distinguish **current reality** from **future intent**.
