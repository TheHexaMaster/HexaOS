# HexaOS Source Architecture

## Purpose

This document describes the **current source architecture** of HexaOS based on the reviewed `src1621` archive and the architectural decisions agreed during the latest audit.

It is not a frozen doctrine. It must track the real codebase and be updated whenever the implementation materially changes.

HexaOS should remain:

- explicit,
- compact in concept,SS
- easy to audit,
- easy to extend,
- resistant to layer mixing,
- honest about what is current versus what is only planned.

---

## Core Principle

HexaOS is not built around heavy class hierarchies or framework-style abstraction.

It is built around **clear ownership**:

- **Core** holds the permanent system skeleton and the mandatory foundational runtime services.
- **Adapters** bridge HexaOS to external APIs, SDK layers, framework backends, or low-level platform resources.
- **Handlers** own internal HexaOS domains and their policy.
- **Drivers** implement reusable device/protocol support above adapters.
- **Services** implement composed runtime functions above handlers, drivers, and adapters.
- **Modules** are top-level cooperative lifecycle participants registered into the runtime dispatcher.
- **Commands** provide frontend-agnostic command dispatch.

The architecture should prefer **clear responsibility** over naming purity.

---

## Important Clarification

HexaOS must distinguish between:

1. **core**,
2. **mandatory for the system to run**,
3. **optional runtime feature**.

These are not the same thing.

A component may be mandatory for the firmware without being an optional module.

In the current codebase, **Core is intentionally broader than just a tiny skeleton**. It already includes mandatory foundational services such as:

- boot orchestration,
- system runtime loop,
- time service,
- configuration service,
- state service,
- local user interface bridge,
- logging,
- RTOS integration,
- panic path,
- module lifecycle registry.

This is the current architectural reality and is considered acceptable.

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
      littlefs_adapter.*
      nvs_adapter.*
      rtos_adapter.*

    commands/
      command_builtin.*
      command_engine.*

    core/
      boot.*
      config.*
      log.*
      module_registry.*
      panic.*
      rtos.*
      runtime.*
      state.*
      system_loop.*
      time.*
      user_interface.*

    drivers/
      (currently empty)

    handlers/
      littlefs_handler.*
      user_interface_handler.*

    modules/
      mod_berry.cpp
      mod_lvgl.cpp
      mod_storage.cpp
      mod_web.cpp
```

Notes about the current tree:

- There is **no `hexaos.h`** in the reviewed archive.
- `boot.*` and `system_loop.*` already exist in `system/core/`.
- `config.*` and `state.*` already live in `system/core/`.
- `mod_system.cpp` no longer exists.
- `nvs_config_handler.*` and `nvs_state_handler.*` no longer exist.
- `nvs_store.*` no longer exists.
- `littlefs_adapter.*` now exists and isolates the raw LittleFS backend boundary from the filesystem handler.
- The `drivers/` directory exists but is not populated yet.
- The `services/` directory is now an **approved architectural layer**, but it is not present in the current reviewed tree yet.

---

## Current Runtime Flow

The current runtime flow in the reviewed archive is:

### `setup()`

- `BootInit()`
- `ModuleInitAll()`
- `ModuleStartAll()`

### `BootInit()` in `system/core/boot.cpp`

- RTOS init
- log init
- user interface init
- time init
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

### `loop()`

- `SystemLoop()`
- `ModuleLoopAll()`
- primitive 100 ms dispatch through `millis()`
- primitive 1 s dispatch through `millis()`

### `SystemLoop()` in `system/core/system_loop.cpp`

- uptime update
- `UserInterfaceLoop()`
- `StateLoop()`

### `SystemEvery100ms()`

- currently empty

### `SystemEverySecond()`

- `HeartBeatTick()`

This means the current system is **not purely module-driven**.

Two important runtime parts intentionally bypass the module layer because they are treated as mandatory system runtime services:

- the local control plane (`UserInterfaceLoop()`),
- the delayed state-commit scheduler (`StateLoop()`).

That is the current intended design.

---

## Current Layer Definitions

### 1. Core

`system/core/`

Core currently contains both the permanent system skeleton and the mandatory foundational services.

Current real Core responsibilities:

- boot orchestration,
- runtime flags/state container,
- panic path,
- RTOS wrapper,
- time service,
- logging,
- module registry and lifecycle fan-out,
- mandatory local control-plane bridge,
- configuration service,
- runtime state service,
- mandatory system runtime loop.

**Core is not a feature module layer.**

It is the always-present system foundation.

---

### 2. Adapter

`system/adapters/`

An adapter is the boundary to something external.

Current real adapters:

- `console_adapter.*` -> local console transport/backend boundary
- `rtos_adapter.*` -> FreeRTOS backend boundary
- `nvs_adapter.*` -> NVS persistence backend boundary
- `littlefs_adapter.*` -> LittleFS/Arduino FS backend boundary

Adapter rule:

- an adapter should bridge one external/backend/resource boundary,
- it should not become the owner of broad unrelated HexaOS domain policy.

#### Audited exception: `nvs_adapter.*`

The current `nvs_adapter.*` is a **deliberate audited hybrid**.

It currently contains not only raw NVS read/write helpers, but also:

- store selection,
- partition mapping,
- namespace mapping,
- cached handles,
- store open/format lifecycle.

This is accepted **for now** because:

- the NVS code is still small,
- splitting it further would currently add ceremony without enough real architectural gain,
- both `config` and `state` already own their own domain policy above it.

This is therefore a **documented exception**, not the default rule for future adapters.

---

### 3. Handler

`system/handlers/`

A handler owns an internal HexaOS domain.

Current real handlers:

- `littlefs_handler.*` -> filesystem domain and file policy above LittleFS
- `user_interface_handler.*` -> shell/session/input/prompt domain

Handler rule:

- a handler owns validation, state, policy, and stable internal API for a HexaOS domain,
- it is not merely a thin SDK wrapper.

Current important note:

- configuration and runtime state are **no longer handlers**. They now belong to `system/core/` as system services.

---

### 4. Driver

`system/drivers/`

The directory already exists but is not yet populated in the reviewed archive.

Planned meaning:

- reusable device/protocol implementations above adapters.

Examples for the future:

- I2C device drivers,
- RTC drivers,
- EEPROM/FRAM drivers,
- IO expander drivers,
- display controller drivers,
- touch controller drivers,
- sensor drivers,
- camera drivers,
- Modbus protocol drivers.

Driver rule:

- a driver owns device/protocol behavior,
- it is not the owner of web/UI/runtime feature policy,
- it should be reusable by services and modules.

---

### 5. Service

`system/services/` (approved layer, not present yet)

A service is a composed runtime function above handlers, drivers, and adapters.

Examples for the future:

- web service,
- telemetry or datapoint service,
- OTA service,
- MQTT service,
- Home Assistant discovery service,
- polling/acquisition service,
- async HTTP client service,
- history recorder service,
- network service,
- time synchronization service,
- automation/rules service.

Service rule:

- a service may coordinate multiple handlers, drivers, and adapters,
- a service is not automatically core,
- a service is often the right home for runtime feature logic that is too large for a handler and too domain-specific for core.

---

### 6. Module

`system/modules/`

A module is a cooperative optional lifecycle participant.

Current modules:

- `mod_storage.cpp`
- `mod_web.cpp`
- `mod_lvgl.cpp`
- `mod_berry.cpp`

Current module reality:

- the module layer is still early,
- the listed modules are mostly lifecycle shells/placeholders,
- modules are currently compiled from `.cpp` only; there are no dedicated module headers yet.

Module rule:

- a module moves an optional feature in time,
- it should not automatically become the owner of a domain just because it has periodic hooks,
- mandatory system runtime work belongs in Core, not in a fake module,
- the main feature logic for a large subsystem will often live in services, not directly in the module file.

---

### 7. Commands

`system/commands/`

Commands provide a frontend-agnostic command surface.

Current command layer responsibilities:

- command registry,
- command lookup,
- line execution,
- help/inspection/mutation commands for config/state/log/files/runtime/time.

Command rule:

- commands trigger internal APIs,
- commands do not own domain state or persistence rules.

Current practical note:

- `command_builtin.cpp` is still a monolithic built-in command collection.
- That is acceptable for now.
- The command registry size is **not considered an architecture-defining limitation**. It may remain large rather than artificially constrained, and command help structure can be redesigned later if needed.

---

## Classification Rules for Future Components

These rules should be used whenever a new HexaOS subsystem is added.

### Put it in **Core** only if:

- the system cannot boot without it,
- the system cannot keep its mandatory runtime skeleton without it,
- it must exist in every meaningful HexaOS build.

Examples:

- boot,
- system loop,
- log,
- time,
- panic,
- config,
- state,
- module registry,
- mandatory local control-plane bridge.

### Put it in an **Adapter** if:

- it is mainly a bridge to ESP-IDF, Arduino, an external library, or a low-level hardware/backend resource,
- its job is mostly boundary translation.

Examples:

- UART,
- I2C,
- SPI,
- LittleFS,
- NVS,
- Wi-Fi backend,
- Ethernet backend,
- HTTP server backend wrapper.

### Put it in a **Handler** if:

- it owns an internal HexaOS domain,
- it holds validation, state, and policy,
- it should present a stable internal API to other HexaOS layers.

Examples:

- user interface handler,
- filesystem handler,
- future session handler,
- future auth handler,
- future datapoint domain owner.

### Put it in a **Driver** if:

- it represents a reusable implementation for a device, controller, or protocol,
- it sits above adapters and below higher feature logic.

Examples:

- sensor drivers,
- display controller drivers,
- touch drivers,
- camera drivers,
- Modbus drivers.

### Put it in a **Service** if:

- it coordinates multiple handlers, drivers, or adapters,
- it implements a runtime feature rather than a low-level boundary,
- it is important, but still not part of the permanent system skeleton.

Examples:

- web service,
- telemetry/datapoint service,
- MQTT service,
- OTA service,
- polling service,
- history service,
- time synchronization service.

### Put it in a **Module** if:

- it is a top-level optional lifecycle participant,
- it needs `Init/Start/Loop/Every100ms/EverySecond` hooks,
- it represents a feature family rather than a single device driver.

Examples:

- web module,
- LVGL module,
- Berry module,
- MQTT module,
- OTA module.

### Important rule about **engines**

`engine` is **not** a separate architectural layer.

The word may be used in names, but architecturally an engine must still be classified as one of:

- service,
- driver,
- handler-internal helper,
- module-internal helper.

HexaOS should not introduce `engine` as a separate top-level layer.

---

## Planned Feature Placement

The following mapping is agreed as the current architectural direction for major future HexaOS features.

### 1. Webserver / user web control plane

Primary placement:

- **Module**: yes -> `mod_web`
- **Service**: yes -> main web runtime logic
- **Handler**: likely -> sessions, terminal policy, assets, auth, request-domain helpers
- **Adapter**: likely -> HTTP backend adapter if wrapping a concrete server backend
- **Core**: no

Web is an optional feature running above networking.

Web should eventually expose:

- config inspection and mutation,
- state inspection and mutation,
- command terminal,
- future Berry console,
- filesystem operations,
- sensor state views,
- future camera/display related views where applicable.

Important rule:

- web is **not** the owner of the camera, filesystem, or telemetry domain,
- web only exposes those subsystems.

### 2. Sensor support and sensor drivers

Primary placement:

- **Drivers**: yes -> per device or protocol
- **Adapters**: yes -> bus/resource boundary
- **Services**: yes -> aggregation, registration, exposure to the rest of the system
- **Modules**: usually no per sensor
- **Core**: no

Important rule:

- an individual sensor should normally be a **driver**, not a standalone module.

### 3. Display drivers and touch drivers

Primary placement:

- **Drivers**: yes -> display controller and touch controller drivers
- **Adapters**: yes -> SPI/I2C/RGB/MIPI/CSI resource boundaries
- **Services**: yes -> display stack, LVGL integration, touch routing, registry/probe logic
- **Module**: yes -> `mod_lvgl` as lifecycle shell for the GUI/display stack
- **Core**: no

Important rule:

- LVGL itself is not a driver; it belongs to service-level integration.

### 4. OTA update system

Primary placement:

- **Service**: yes -> OTA workflow and policy
- **Module**: likely yes -> lifecycle shell if periodic checks/watchers exist
- **Adapters**: yes -> flash/partition/network backend boundaries
- **Handlers**: possible -> manifest/update domain if needed
- **Core**: no, except boot compatibility requirements

Important rule:

- OTA is not core; core only needs to remain compatible with OTA/rollback/safeboot requirements.

### 5. MQTT / Web API / external integrations / Home Assistant

This area should be split by role.

#### MQTT

- **Module**: yes -> `mod_mqtt` when introduced
- **Service**: yes -> publish/subscribe/reconnect/discovery logic
- **Adapters**: yes -> backend/network/TLS boundaries where needed
- **Core**: no

#### Home Assistant integration

- **Service**: yes -> typically above MQTT
- **Core**: no

#### Web API

- inbound API belongs under the web stack,
- outbound API client belongs in its own service/module when needed.

Important rule:

- a protocol is not automatically a module; it becomes a module only when it has its own lifecycle and runtime management needs.

### 6. Polling systems / Modbus / template-based acquisition

Primary placement:

- **Drivers**: yes -> Modbus RTU/TCP and similar protocol drivers
- **Services**: yes -> polling/acquisition/template execution logic
- **Adapters**: yes -> UART/RS485/TCP boundaries
- **Core**: no

Important rule:

- polling logic must not be pushed into the Modbus driver itself,
- the driver communicates,
- the service decides what to poll, when, and how results are routed.

### 7. Internal sensor / datapoint database

This is expected to become one of the central HexaOS subsystems.

Primary placement:

- **Service**: yes -> primary owner of the runtime datapoint model
- **Handler**: likely yes -> internal datapoint domain/policy owner
- **Module**: no as primary owner
- **Core**: no
- **Driver**: no

Recommended conceptual role:

- hold current values,
- hold metadata,
- unify read/write capabilities,
- carry timestamps, units, flags, source information,
- serve as the central exchange layer for web, console, MQTT, polling, automation, and history.

Important rule:

- this subsystem should be treated as a major service, not a random helper and not a core dump bucket.

### 8. Other likely future subsystems

Likely future additions and their expected placement:

- **network domain** -> handler + services + adapters
- **time synchronization (NTP/RTC policy)** -> service above core time
- **history recorder** -> service + handler
- **authentication/access control** -> handler + service
- **automation/rules/schedules** -> service, possibly with its own module later
- **script/package/app management** -> Berry-related module + services + filesystem integration

---

## Current Transitional Reality

### User interaction is already no longer a module

Current split:

- `core/user_interface.*` -> mandatory runtime bridge and transport polling
- `handlers/user_interface_handler.*` -> shell/session/prompt/command handling
- `adapters/console_adapter.*` -> backend transport boundary

This split is considered correct and should remain.

### Multi-endpoint log/shell routing is intentionally deferred

The current user interface path is designed around the present single local endpoint.

Future multi-endpoint work such as:

- web console,
- telnet console,
- multiple simultaneous sinks,
- broader sink routing,

is explicitly **deferred until another endpoint actually exists**.

HexaOS should not introduce speculative routing complexity before it is needed.

### `state.cpp` is intentionally large for now

The current state subsystem is large because it already owns:

- static schema state,
- runtime state registry,
- runtime catalog,
- delayed commit scheduling,
- storage reporting,
- write restriction policy,
- owner classes,
- value conversion helpers.

This file has already been refactored and heavily tested.

The current project decision is:

- **do not split it further without evidence-based need**,
- **do not refactor it again just for size**,
- continue treating it as a single domain-owned subsystem unless new responsibilities start leaking into it.

---

## Agreed Direction: User Interface

HexaOS should continue using the current split that exists in the codebase.

### Core bridge

`core/user_interface.*`

Should remain:

- small,
- mandatory,
- transport/polling oriented,
- free of shell policy.

### Domain owner

`handlers/user_interface_handler.*`

Should remain the owner of:

- prompt state,
- line editing,
- session state,
- redraw policy,
- command dispatch bridge,
- future multi-endpoint shell/session policy.

### Transport/backend boundary

`adapters/console_adapter.*`

Currently owns the selected local console backend.

Future evolution may split this further if HexaOS introduces a broader shared UART/USB transport layer, but that is not required yet.

### Remote control endpoints

Future remote or hosted control channels should become **services** that feed the same `user_interface_handler` domain rather than owning separate shell logic.

Examples:

- `web_console_service.*`
- `telnet_console_service.*`

---

## Agreed Direction: Persistence

HexaOS currently uses a simple persistence model:

- `core/config.*` owns configuration logic,
- `core/state.*` owns runtime state logic,
- `adapters/nvs_adapter.*` provides the current persistence backend,
- `adapters/littlefs_adapter.*` provides the current LittleFS backend boundary,
- `handlers/littlefs_handler.*` owns filesystem policy and file-domain logic.

Current rule:

- persistence **policy** belongs to `config`, `state`, and filesystem handlers,
- persistence **backend implementation** currently lives in `nvs_adapter.*` and `littlefs_adapter.*`.

The present NVS hybrid remains acceptable until the codebase grows enough to justify separating:

- raw NVS binding,
- NVS backend manager,
- higher persistence services.

That split is **not required yet**.

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

- `mod_network.cpp`
- `web_service.*`
- `mqtt_service.*`
- `async_http_client_service.*`
- `ota_service.*`

Important design point:

**Web is not the network domain itself.**
It is a service running above networking.

---

## Recommended Near-Term Tree Direction

```text
src/
  headers/
    hx_build.h
    hx_config.h

  hexaos.cpp

  system/
    core/
      boot.*
      config.*
      log.*
      module_registry.*
      panic.*
      rtos.*
      runtime.*
      state.*
      system_loop.*
      time.*
      user_interface.*

    adapters/
      console_adapter.*
      littlefs_adapter.*
      nvs_adapter.*
      rtos_adapter.*
      uart_adapter.*            (future, only if needed)
      i2c_adapter.*             (future)
      spi_adapter.*             (future)
      wifi_adapter.*            (future)
      ethernet_adapter.*        (future)
      lwip_adapter.*            (future)
      http_server_adapter.*     (future, only if needed)

    handlers/
      littlefs_handler.*
      user_interface_handler.*
      network_handler.*         (future)
      datapoint_handler.*       (future)
      auth_handler.*            (future)

    drivers/
      sensor_*.cpp/h            (future)
      display_*.cpp/h           (future)
      touch_*.cpp/h             (future)
      camera_*.cpp/h            (future)
      modbus_*.cpp/h            (future)

    services/
      web_service.*
      web_console_service.*
      mqtt_service.*
      ha_discovery_service.*
      ota_service.*
      polling_service.*
      datapoint_service.*
      history_service.*
      async_http_client_service.*
      automation_service.*

    commands/
      command_engine.*
      command_builtin.*

    modules/
      mod_storage.cpp
      mod_web.cpp
      mod_lvgl.cpp
      mod_berry.cpp
      mod_mqtt.cpp              (future)
      mod_ota.cpp               (future)
      mod_network.cpp           (future)
```

This is the **direction**, not a claim that the current tree already implements all of it.

---

## Dependency Direction

### Allowed

- Core may depend on shared headers and core-internal helpers.
- Adapters may depend on external APIs and internal shared headers.
- Handlers may depend on adapters, core, and shared headers.
- Drivers may depend on adapters and shared headers.
- Services may depend on handlers, drivers, adapters, and shared headers.
- Modules may depend on services, handlers, core, and shared headers.
- Commands may depend on services, handlers, core, and shared headers.

### Discouraged

- Core depending on optional feature modules.
- Adapters containing broad unrelated domain policy.
- Handlers becoming raw SDK wrappers.
- Drivers owning web/UI/runtime feature policy.
- Modules owning multiple unrelated layers at once.
- Commands owning domain state.
- Prematurely splitting small subsystems just to satisfy naming purity.
- Creating `engine` as a separate top-level architecture layer.

---

## The current accepted architectural stance

- keep the architecture explicit,
- keep exceptions documented,
- avoid speculative abstraction,
- prefer real evidence over refactoring for aesthetics,
- use the classification rules above when introducing future subsystems.
