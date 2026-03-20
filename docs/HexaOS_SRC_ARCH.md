# HexaOS Architecture

## Purpose

This document defines the **current architectural state** of HexaOS based on the latest source archive and records the **agreed target direction** for the next structural refactor.

It is not intended as dogma. It is a working architecture definition that must follow the real codebase and evolve when the design becomes clearer.

HexaOS should remain:

- explicit,
- small in concept,
- easy to audit,
- easy to extend,
- resistant to layer mixing.

---

## Core Principle

HexaOS is not built around classes or framework-style abstraction.

It is built around **clear ownership**:

- **Core** holds the permanent system foundation and runtime skeleton.
- **Adapters** bridge HexaOS to external APIs, SDKs, framework layers, or low-level hardware backends.
- **Handlers** own internal HexaOS domains and their policy.
- **Modules** are cooperative runtime participants registered into the lifecycle dispatcher.
- **Commands** provide user/operator command dispatch.
- **Drivers** will represent reusable device/protocol implementations above adapters.
- **Services** will represent composed runtime services built on top of handlers, drivers, and adapters.

---

## Important Clarification

HexaOS must distinguish between:

1. **core**
2. **mandatory for a given product build**
3. **optional runtime feature**

These are not the same thing.

A component may be mandatory for a product profile without belonging to Core.

**Core** must stay small.

---

## Current Source Tree (latest archive)

```text
src/
  headers/
    hx_build.h
    hx_config.h
    include/
      pre_build.h
      pos_build.h
      pre_config.h
      pos_config.h

  hexaos.cpp
  hexaos.h

  system/
    core/
      boot.*
      log.*
      module_registry.*
      panic.*
      rtos.*
      runtime.*

    adapters/
      console_adapter.*
      nvs_adapter.*
      rtos_adapter.*

    commands/
      command_engine.*
      command_builtin.*

    handlers/
      littlefs_handler.*
      nvs_config_handler.*
      nvs_state_handler.*

    modules/
      mod_system.*
      mod_console.*
      mod_storage.*
      mod_web.*
      mod_lvgl.*
      mod_berry.*

    drivers/
      (currently empty)
```

---

## Current Runtime Flow

The current runtime flow in the latest archive is:

### setup()

- RTOS init
- Log init
- Boot init

### BootInit()

- board / reset / chip boot reporting
- Config init
- Config load
- Config apply
- State init
- Files init
- State load
- Files mount
- Command init
- Module init all
- Module start all

### loop()

- `ModuleLoopAll()`
- `StateLoop()`
- `ModuleEvery100ms()`
- `ModuleEverySecond()`

This means the current model is **mostly module-driven**, but not absolutely. `StateLoop()` still runs directly from the main loop.

---

## Layer Definitions

### 1. Core

`system/core/`

Core contains the permanent HexaOS foundation.

Current real Core responsibilities:

- boot orchestration,
- runtime values,
- panic,
- logging,
- RTOS entry,
- module registry and lifecycle fan-out.

Core should remain free of feature-domain policy such as web logic, storage policy, network policy, or device-specific business rules.

**Core is the system skeleton, not the feature layer.**

---

### 2. Adapter

`system/adapters/`

An adapter is the boundary to something external.

Examples:

- Arduino or ESP-IDF APIs,
- FreeRTOS,
- NVS,
- USB Serial/JTAG backend,
- UART backend,
- Wi-Fi stack,
- Ethernet stack,
- TCP/IP stack,
- external storage or peripheral libraries.

**Rule:** an adapter should exist once per backend/resource boundary, not once per use-case.

Good examples:

- `uart_adapter.*`
- `usb_cdc_jtag_adapter.*`
- `wifi_adapter.*`
- `ethernet_adapter.*`
- `lwip_adapter.*`

Bad pattern:

- creating `*_console_adapter` and `uart_adapter` for the same UART backend.

That would duplicate the same external boundary under two different names.

---

### 3. Handler

`system/handlers/`

A handler owns an internal HexaOS domain.

A handler is where policy, domain semantics, validation, domain state, and reusable internal API belong.

Current real handlers already show this well:

- `nvs_config_handler.*`
- `nvs_state_handler.*`
- `littlefs_handler.*`

Future examples:

- `uart_handler.*`
- `network_handler.*`
- `user_interface_handler.*`
- `files_handler.*`

**Rule:** a handler owns a HexaOS domain. It is not just a thin wrapper around an SDK.

---

### 4. Module

`system/modules/`

A module is a cooperative runtime participant.

Modules currently participate in:

- init,
- start,
- loop,
- every-100ms,
- every-second.

A module provides runtime heartbeat, but should not become a dump zone for policy, low-level backend glue, and user-facing logic all at once.

**Rule:** a module moves the system in time. It should not automatically own a domain just because it has a loop.

---

### 5. Commands

`system/commands/`

Commands provide a frontend-agnostic command surface.

The command layer should:

- parse command text,
- dispatch to the correct internal API,
- return output through an abstract output sink.

The command layer should not become the owner of the system model.

Current code already follows this direction well through `command_engine.*` and `command_builtin.*`.

---

### 6. Drivers

`system/drivers/`

The `drivers/` directory already exists and should become the layer for **device/protocol implementations** above low-level adapters.

Examples:

- RTC chip drivers,
- EEPROM drivers,
- display drivers,
- IO expander drivers,
- sensor protocol drivers.

A driver should not talk directly to product policy.

Example direction:

- `ds3231_driver.*` -> uses `i2c_adapter.*`
- `at24c32_driver.*` -> uses `i2c_adapter.*`
- `xl9535_driver.*` -> uses `i2c_adapter.*`

---

### 7. Services

`system/services/` (proposed)

A service is a composed runtime function built on top of handlers, drivers, and adapters.

Services are useful when something is not the owner of a full HexaOS domain but is more than a thin backend bridge.

Examples:

- web console service,
- telnet console service,
- async HTTP client service,
- web server service,
- hosted API service.

A service is not automatically a handler.

---

## Current Transitional Problem Areas

### mod_console is a hybrid

In the current codebase, `mod_console.*` is not a clean single-role module.

It currently mixes:

- runtime polling,
- prompt redraw logic,
- shell line editing,
- interactive state,
- command dispatch glue,
- direct use of `console_adapter`.

This should be treated as a **transitional implementation**, not as the final architectural shape.

### modules are not yet the final stable layer model

`mod_system` is small and clear.

`mod_storage`, `mod_web`, `mod_lvgl`, and `mod_berry` are still largely placeholder lifecycle shells.

This means the module layer is still under construction and can be redesigned cleanly without breaking a mature final model.

---

## Agreed Direction: User Interface

### Why console should not remain a standalone module identity

The system should not think in terms of:

- console module,
- web console module,
- telnet module,
- LVGL control module.

Those are all just **different control-plane / user-interface endpoints**.

The architectural concept should be broader than `console`.

### Agreed target model

HexaOS should move toward a **mandatory control-plane nucleus** with a clear separation of concerns.

#### Core

Add:

- `core/user_interface.*`

This should be the mandatory runtime bridge for the local and attached user-control plane.

It should:

- participate directly in the core-controlled execution path,
- attach local boot-time control transports if enabled,
- poll mandatory local control endpoints,
- route interaction to the UI domain owner,
- remain small.

It should **not** become the place where full shell semantics live.

#### Handler

Add:

- `handlers/user_interface_handler.*`

This should own:

- line editing,
- session state,
- prompt state,
- input/output routing policy,
- command dispatch bridge,
- redraw behavior,
- future multi-endpoint session logic.

This is the real owner of the user interaction domain.

#### Resource sharing

For shared resources such as UART, the model should be:

- `uart_adapter.*` -> low-level UART backend bridge
- `uart_handler.*` -> UART domain owner / channel manager
- `user_interface` uses `uart_handler` when a UART-backed local console is configured

This avoids duplicating the UART backend behind multiple different adapter names.

#### External control endpoints

Future remote or hosted control channels should become **services**, not extra core logic and not duplicated handlers.

Examples:

- `web_console_service.*`
- `telnet_console_service.*`

These services should feed the same `user_interface_handler` domain rather than owning their own shell implementation.

---

## Agreed Direction: Networking

Networking should be split into three separate concepts.

### 1. Uplink backends

Adapters:

- `wifi_adapter.*`
- `ethernet_adapter.*`
- optionally `lwip_adapter.*` or another TCP/IP boundary adapter

These are backend/resource boundaries only.

### 2. Network domain owner

Handler:

- `network_handler.*`

This should own:

- active uplink state,
- network readiness,
- addressing state,
- reconnect / fallback policy,
- uniform API for the rest of HexaOS.

### 3. Services above network

Module + services pattern:

- `mod_network.*` -> runtime heartbeat for network domain
- `mod_services.*` -> runtime heartbeat for hosted/client services
- `web_service.*`
- `async_http_client_service.*`
- `mqtt_service.*`
- other future services

Important design decision:

**Web is not the network domain itself.**
It is a service that runs above networking.

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
    include/
      pre_build.h
      pos_build.h
      pre_config.h
      pos_config.h

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
      nvs_adapter.*
      rtos_adapter.*
      usb_cdc_jtag_adapter.*
      uart_adapter.*
      wifi_adapter.*
      ethernet_adapter.*
      lwip_adapter.*

    handlers/
      nvs_config_handler.*
      nvs_state_handler.*
      littlefs_handler.*
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
      mod_system.*
      mod_network.*
      mod_services.*
      mod_storage.*
      mod_lvgl.*
      mod_berry.*
```

This is the **target direction**, not a claim that the current archive already implements all of it.

---

## Dependency Direction

### Allowed

- Core may depend on shared system headers and core-internal helpers.
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
- Consumer-specific adapters duplicating a generic backend adapter.
- Modules owning multiple unrelated layers at once.
- Commands owning domain state.

---

## Naming Rules

### Use `*_adapter` when the file:

- bridges HexaOS to an external API, SDK, framework, or hardware backend,
- represents one external/resource boundary once.

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

### 2. Duplicate adapters for the same backend

Do not create separate adapters for different consumers of the same UART/TCP/USB backend.

### 3. Mandatory == Core

Do not move a feature into Core only because a particular product build requires it.

### 4. Service logic hidden inside adapters

An adapter must not become the place where reconnect policy, session logic, application routing, or business behavior accumulate.

### 5. Domain logic inside commands

Commands should trigger domain APIs, not become the domain.

---

## Current Working Interpretation

As of the latest archive:

- Core skeleton is already meaningful and usable.
- Config and state are already strong domain handlers.
- LittleFS is functionally handler-like today.
- Module architecture is still transitional.
- `mod_console` is still a hybrid and should not define the final model.
- `drivers/` already exists and should be activated architecturally.
- networking/services/UI structure is still open and can be corrected cleanly now.

---

## Final Summary

HexaOS should be understood with this mental model:

- **Core** = permanent system skeleton
- **Adapter** = external/backend boundary
- **Handler** = owner of a HexaOS domain
- **Driver** = reusable device/protocol implementation above adapters
- **Service** = composed runtime function above handlers/drivers/adapters
- **Module** = lifecycle participant
- **Commands** = frontend-agnostic command surface

Additional agreed direction:

- user interaction should evolve from a `console`-named module into a broader **user interface / control-plane architecture**,
- shared resources such as UART must not be wrapped twice through use-case-specific adapters,
- networking should be treated as a domain distinct from hosted services running above it,
- the architecture document must follow the real code and remain adjustable as HexaOS evolves.
