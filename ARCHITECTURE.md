# HexaOS Architecture

## Purpose

This document defines the long-term architectural model of HexaOS.

It is a **whitepaper**, not a moving review of whichever source files exist at a particular moment. Its purpose is to preserve architectural consistency across future development, regardless of repository growth, feature count, or implementation detail changes.

This document defines:

- the permanent layer model of HexaOS,
- the ownership rules for future components,
- the allowed dependency directions,
- the lifecycle model,
- the distinction between core, services, modules, handlers, adapters, drivers, and commands,
- the approved handling of early-boot integrations,
- and canonical examples of how future subsystems must be integrated.

HexaOS must remain:

- explicit,
- layered,
- auditable,
- extensible,
- conservative in abstraction,
- and resistant to ownership mixing.

---

## Architectural Identity

HexaOS is not organized around deep class hierarchies or framework-style inversion.

HexaOS is organized around **clear ownership boundaries**.

The central question is always:

> Which layer owns this responsibility?

A component belongs to a layer only when that layer is the correct owner of its policy, lifecycle, and dependency direction.

---

## Global Architectural Graph

```text
+--------------------------------------------------------------+
|                         User Surfaces                         |
|   Console   |   Web UI   |   Web Terminal   |   Future UI     |
+------------------------------+-------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|                           Commands                            |
| frontend-agnostic command execution and dispatch              |
+------------------------------+-------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|                            Core                               |
| boot | system loop | runtime | time | log | panic |           |
| module registry | config | state | user-interface bridge      |
+------------------+-------------------+------------------------+
                   |                   |
                   |                   |
                   v                   v
+----------------------+     +----------------------------------+
|       Services       |     |            Handlers              |
| composed runtime     |     | internal HexaOS domain owners    |
| functions and policy |     | validation, stable internal API  |
+-----------+----------+     +------------------+---------------+
            |                                   |
            |                                   |
            v                                   v
+--------------------------------------------------------------+
|                            Drivers                            |
| reusable device / protocol implementations                    |
+------------------------------+-------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|                           Adapters                            |
| bridges to ESP-IDF / Arduino / libraries / buses / backends   |
+------------------------------+-------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|               Hardware / SDK / External Backends              |
| I2C | SPI | UART | FS | NVS | Wi-Fi | Ethernet | HTTP | etc.  |
+--------------------------------------------------------------+
```

Interpretation:

- **Core** owns the permanent system skeleton.
- **Handlers** own internal HexaOS domains.
- **Services** compose runtime behavior across multiple lower layers.
- **Drivers** implement devices and protocols.
- **Adapters** bridge external APIs and resources.
- **Modules** are not shown as a separate execution layer above because they are **lifecycle shells**, not domain owners by default.

---

## Permanent Layer Model

## 1. Core

Core contains the permanent execution skeleton of HexaOS.

Core owns responsibilities without which the system cannot boot, cannot keep a coherent runtime model, or cannot expose its minimal internal control plane.

Core includes concepts such as:

- boot orchestration,
- system loop,
- runtime state container,
- time source of truth,
- logging,
- panic and fatal path,
- module registry and lifecycle fan-out,
- configuration service,
- persistent state service,
- mandatory local control-plane bridge.

Rules:

- Core is **not** a feature layer.
- Core may host foundational mandatory services when they are part of the permanent skeleton.
- Core must remain conservative and small in concept, even when implementation grows.

---

## 2. Adapter

An adapter is the boundary to an external API, framework, SDK, resource, bus, or backend.

Typical adapter examples:

- I2C adapter,
- SPI adapter,
- UART adapter,
- LittleFS adapter,
- NVS adapter,
- Wi-Fi adapter,
- Ethernet adapter,
- HTTP server backend adapter,
- OTA flash adapter.

Rules:

- An adapter bridges **how** something is accessed.
- An adapter does not own HexaOS domain policy.
- An adapter must not become a feature owner merely because it is close to hardware.

### Audited Hybrid Exception

HexaOS allows a hybrid adapter only as an **explicitly audited exception**.

This exception is permitted when:

- the total code is small,
- splitting it further would introduce ceremony without meaningful architectural gain,
- and the mixed ownership remains understandable and reviewable.

This exception must remain rare and intentional.

---

## 3. Handler

A handler owns an internal HexaOS domain.

A handler is the owner of:

- validation,
- internal policy,
- stable domain API,
- internal state of that domain,
- domain-specific rules visible to multiple higher layers.

Typical handler examples:

- user interface handler,
- files handler,
- network handler,
- session handler,
- authentication handler,
- telemetry/datapoint handler.

Rules:

- A handler is not a thin SDK wrapper.
- A handler is an internal HexaOS domain owner.
- A handler may depend on core and adapters.

---

## 4. Driver

A driver is a reusable implementation for a device, controller, or protocol.

Typical driver examples:

- RTC chip driver,
- temperature sensor driver,
- display controller driver,
- touch controller driver,
- Modbus RTU driver,
- CSI camera driver.

Rules:

- A driver owns device/protocol behavior.
- A driver does not own user-facing workflow policy.
- A driver does not automatically deserve its own module.
- Drivers sit above adapters and below handlers/services/modules.

---

## 5. Service

A service is a composed runtime function above handlers, drivers, and adapters.

Services are the correct home for logic that is too rich for a driver, too external for core, and too broad for a single handler.

Typical service examples:

- web service,
- OTA service,
- telemetry publish service,
- MQTT service,
- Home Assistant discovery service,
- polling service,
- time synchronization service,
- history recorder service,
- automation/rules service.

Rules:

- A service owns workflow policy and composed runtime behavior.
- A service may use multiple handlers, drivers, and adapters.
- A service may have boot-phase APIs, runtime APIs, or both.
- A service is not automatically a module.

### Passive and Active Services

Services may be:

#### Passive services
They expose API but do not require their own periodic lifecycle.

Examples:

- time synchronization policy used only on boot and on explicit NTP update,
- file transformation helper service,
- one-shot migration service.

Passive services do **not** require a module.

#### Active services
They own periodic or continuous runtime behavior.

Examples:

- polling engines,
- reconnecting network clients,
- recorder flush loops,
- recurring telemetry publication,
- watchdog or monitoring loops.

Active services may be hosted:

- by a dedicated domain module,
- or by a shared domain orchestrator module.

### Transactional Services

Some services are neither simple passive APIs nor classic periodic pollers. They own long-running transactional workflows such as uploads, exports, migrations, or update jobs.

These services may combine:

- event-driven request entry points,
- internal state machines,
- chunk queues,
- incremental processing,
- and optional RTOS-backed workers.

They still remain services. They do not become core merely because they handle large or time-consuming work.

---

## 6. Module

A module is a **top-level lifecycle orchestrator**.

A module participates in one or more of:

- init,
- start,
- loop,
- every-10-ms,
- every-100-ms,
- every-second.

Rules:

- A module exists to move a domain in time.
- A module is not automatically the owner of all domain policy beneath it.
- Modules are created per **runtime domain**, not per individual device.

### Critical Module Rule

> Modules are domain lifecycle orchestrators, not one-per-driver wrappers.

Correct module examples:

- web module,
- MQTT module,
- polling module,
- I2C sensors module,
- LVGL/display module,
- connectivity module.

Incorrect module examples:

- one module per sensor driver,
- one module per RTC chip,
- one module per display controller,
- one module per protocol codec.

Drivers may exist without modules.
Services may exist without modules.

A module is justified only when a runtime domain requires cooperative lifecycle participation.

A module does not automatically imply a scheduler.

Many modules will remain simple lifecycle participants with no internal scheduling policy beyond cooperative hooks. Scheduling policy is introduced only for domains that must plan multiple jobs, budgets, retries, or phase-distributed work.

---

## 7. Commands

Commands are a frontend-agnostic control surface.

Commands exist so that multiple frontends may reuse the same execution layer:

- console,
- web terminal,
- future Berry terminal,
- future remote command surfaces.

Rules:

- Commands do not own business state.
- Commands trigger domain APIs in core, handlers, and services.
- Commands are not UI transports.
- Commands must remain decoupled from specific frontend implementations.

---

## 8. Engine Naming Rule

The word **engine** may be used in names, but it is **not a top-level architectural category**.

Any component called an engine must still belong to one of the canonical layer types:

- core,
- adapter,
- handler,
- driver,
- service,
- module,
- command support.

This prevents the architecture from creating a parallel undocumented layer model.

---

## Canonical Directory Model

The architectural directory model of HexaOS is:

```text
src/
  headers/
    hx_*.h              # shared public system headers and stable definitions

  hexaos.cpp            # top-level setup()/loop() bridge only

  system/
    core/               # permanent execution skeleton and foundational system services
    interlibs/          # shared internal helper libraries without domain ownership
    adapters/           # bridges to external APIs, SDKs, buses, storage, backends
    handlers/           # internal domain owners
    drivers/            # reusable device and protocol implementations
    services/           # composed runtime functions and policies
    commands/           # frontend-agnostic command execution
    modules/            # domain lifecycle orchestrators
```

Subdirectories may be introduced inside these layers when growth justifies them.

Examples:

```text
system/interlibs/jsonparser/
system/drivers/rtc/
system/drivers/sensors/
system/drivers/display/
system/services/web/
system/services/telemetry/
system/services/polling/
```

### Interlibs

`system/interlibs/` is reserved for small shared internal libraries that are reused across multiple HexaOS layers but do not own a domain, a device, a workflow, or an external backend.

Rules:

- Interlibs may provide parsing, formatting, conversion, or other reusable helper logic.
- Interlibs must remain policy-light and domain-agnostic.
- Interlibs must not become a hidden replacement for handlers, services, or adapters.
- When a helper starts owning domain rules, device behavior, transport behavior, or runtime workflow policy, it no longer belongs in interlibs.

---

## Dependency Direction Rules

### Allowed

- Core may depend on shared headers, interlibs, and other core components.
- Handlers may depend on core, interlibs, and adapters.
- Drivers may depend on interlibs, adapters, and shared headers.
- Services may depend on core, interlibs, handlers, drivers, and adapters.
- Modules may depend on core, interlibs, handlers, services, and drivers.
- Commands may depend on core, interlibs, handlers, and services.

### Forbidden or Strongly Discouraged

- Core depending on optional feature modules.
- Adapters owning domain policy.
- Handlers becoming raw SDK wrappers.
- Drivers owning user-facing workflow policy.
- Modules created per device without real lifecycle need.
- Commands becoming persistence or domain owners.
- UI transports bypassing command/service/domain boundaries when those already exist.

---


## Build-Time Feature Gating

HexaOS supports build-time enabling and disabling of optional feature families.

A build flag that disables a feature means the feature is **architecturally absent**, not merely inactive at runtime.

This rule applies to the entire feature stack.

If an optional feature family is disabled, every layer that exists only to support that feature must also be removed from the build as appropriate:

- adapters,
- handlers,
- drivers,
- services,
- module shells,
- command surfaces,
- UI/API surfaces,
- feature-specific integration glue.

### Canonical Rule

> Build-time feature gating applies to the complete feature family, not only to its top-level entry point.

A disabled feature must not remain partially linked into the binary through stray includes, helper functions, route tables, commands, or backend wrappers.

### Optional Consumers

When another subsystem uses an optional capability only opportunistically, it must depend on the higher-level domain API rather than a concrete backend implementation.

That consumer must be able to handle outcomes such as:

- unavailable,
- unsupported,
- not built,
- backend missing.

Example:

- a web subsystem may continue to exist without a flash filesystem,
- while file-manager or static-asset features are compiled out.

### Hard Requirements

When a subsystem fundamentally requires another feature family, this dependency must be enforced at build time.

Canonical pattern:

```text
feature enabled + required dependency disabled -> build error
```

This prevents silent architectural amputation where the build succeeds but the subsystem is no longer conceptually whole.

Flash filesystem support is a canonical example of this rule, but the rule is general and applies equally to networking, telemetry backends, storage backends, display stacks, and future optional domains.

---

## Initialization Order and Dependency Safety

HexaOS must never rely on accidental module ordering as a hidden dependency mechanism.

### Canonical Rule

> Module initialization order must not be used as dependency resolution.

`Init()` and `Start()` phases are self-initialization phases for a module's own runtime domain. They are not permission to assume that another module has already initialized its services.

### Consequences

A module may:

- initialize its own state,
- register its own routes, commands, handlers, callbacks, or observers,
- prepare its own runtime structures.

A module must not:

- assume another module has already started,
- assume another module has already initialized a service,
- require relative ordering in the module list to function correctly.

### Early Mandatory Dependencies

If a dependency must exist before modules begin their lifecycle, it belongs to one of the following forms:

- core boot initialization,
- or an explicit early-boot service hook called from `BootInit()`.

This is the approved model for truly early requirements.

### Runtime Optional Dependencies

If a dependency is not early-boot critical, it must be handled through explicit readiness and capability checks.

Canonical outcomes include:

- ready,
- not ready,
- unavailable,
- disabled by build,
- backend absent.

This allows services and modules to degrade gracefully without turning the module list into a hidden dependency graph.

### Stability Rule

> A service must not be initialized by accident.

A service must be one of the following:

- boot-ready through explicit boot orchestration,
- explicitly initialized by its owning runtime domain,
- lazily usable through readiness checks,
- or absent because the feature family is not part of the build.

This rule keeps HexaOS auditable and prevents fragile cross-module coupling.

---


## Lifecycle Model

## Top-Level Runtime Shape

The permanent top-level model is:

```text
setup()
  -> BootInit()
  -> ModuleInitAll()
  -> ModuleStartAll()

loop()
  -> SystemLoop()
  -> ModuleLoopAll()
  -> Every10ms dispatch
  -> Every100ms dispatch
  -> EverySecond dispatch
```

Interpretation:

- `hexaos.cpp` remains a thin entry bridge.
- Boot orchestration belongs to core.
- Mandatory runtime housekeeping belongs to core.
- Optional or domain-scoped runtime participants belong to modules.
- The global cadence set must remain intentionally small.

---

## Soft Cadence Model and Domain Scheduling Rules

HexaOS uses a small set of global lifecycle cadence classes.

The canonical cooperative cadence model is:

- `Loop()`
- `Every10ms()`
- `Every100ms()`
- `EverySecond()`

### Meaning of the Cadence Classes

- `Loop()` is opportunistic and does not imply a periodic guarantee.
- `Every10ms()` is the fast cooperative soft-periodic hook.
- `Every100ms()` is the normal periodic hook.
- `EverySecond()` is the slow maintenance hook.

### Canonical Rule

> The global cadence model must remain small. Fine-grained timing must not be expressed by multiplying global dispatch classes.

HexaOS must not grow separate global hooks for arbitrary intervals such as 3 s, 5 s, 10 s, or per-device timing needs.

Those timing needs belong to domain-level scheduling policy.

### Domain Scheduling Rule

When a runtime domain must manage many jobs with different intervals, retry windows, phase offsets, or budgets, it must use a **domain scheduler** hosted by the owning service or module.

Examples:

- sensor polling domains,
- protocol polling domains,
- history/flush domains,
- telemetry publication domains.

A domain scheduler may maintain concepts such as:

- interval,
- next due timestamp,
- phase offset,
- enabled state,
- priority,
- retry/backoff,
- bus or backend affinity,
- per-tick budget.

### Scheduler Scope Rule

> Many modules do not imply many unique schedulers.

HexaOS may contain many modules while only a small number of runtime domains actually require scheduling policy.

The approved model is:

- one shared scheduling mechanism may exist as a reusable primitive,
- multiple domains may instantiate or reuse that mechanism,
- only domains with real planning needs should host scheduler state.

This avoids two opposite failures:

- one global mega-scheduler that becomes a new monolith,
- many ad-hoc per-module or per-driver `millis()` schedulers with inconsistent behavior.

### Driver Timing Rule

Drivers must not become owners of global runtime scheduling.

A driver may perform device-local timing only when required for a device transaction, but periodic orchestration belongs to the owning service or runtime domain.

### Load-Spreading Rule

A dispatch tick is a chance to advance work, not permission to process everything at once.

Therefore:

- equal-period jobs should be phase-distributed rather than aligned to a single edge,
- bus-heavy work must be budgeted,
- per-bus and per-domain pacing is encouraged,
- periodic dispatch must not create artificial once-per-second bursts on shared buses.

### Fast Cadence Rule

`Every10ms()` is the approved global fast cooperative cadence.

A general-purpose global `Every5ms()` hook is not part of the canonical architecture by default. If a future domain truly requires a tighter cadence, that requirement should first be evaluated against:

- domain scheduling,
- driver-local timing,
- interrupt/timer mechanisms,
- or RTOS-backed workers.

Only a clearly justified and audited future extension may introduce a tighter global cadence class.

---

## Long-Running Transactional Processing

HexaOS distinguishes between periodic scheduling and long-running transactional work.

Examples of long-running transactional work include:

- OTA upload,
- large file upload,
- large file copy or export,
- import/migration jobs,
- large network download or transfer,
- storage-to-storage streaming operations.

These operations must not be modeled as ordinary periodic polling tasks.

### Canonical Rule

> Periodic scheduling and long-running transactional processing are different architectural categories.

Periodic scheduling exists to decide **when** to run recurring work.

Transactional processing exists to move a bounded long-running operation forward through states such as:

- begin,
- receiving,
- queued,
- writing,
- verifying,
- finalizing,
- done,
- aborted,
- error.

### Approved Transaction Model

The approved architecture for long-running work is:

- the owning **service** defines the transaction policy and state machine,
- the user-facing surface submits work to that service,
- the service advances the transaction incrementally,
- raw backend access belongs to adapters,
- persistent domain policy belongs to handlers when justified.

This model may use:

- state machines,
- queues,
- ring buffers,
- chunked processing,
- bounded per-iteration budgets.

### Responsiveness Rule

> Long operations must be chunked and budgeted.

A long transaction must not monopolize the cooperative runtime by processing an unbounded amount of work in a single callback or single loop turn.

This rule exists to preserve:

- UI responsiveness,
- command responsiveness,
- network responsiveness,
- watchdog safety,
- and predictable system behavior under load.

### RTOS Worker Rule

RTOS is the approved execution mechanism when a long-running transaction or backend interaction would otherwise compromise responsiveness or create unacceptable blocking behavior.

However:

- RTOS is an execution tool, not a new ownership layer,
- the owning service still owns the workflow,
- the worker task only executes queued or chunked work,
- adapters remain the raw boundary to storage, flash, buses, or network backends.

### Canonical Rule

> In HexaOS, services own long-running workflows; RTOS workers execute them.

This means:

- an OTA or upload task must not become the owner of update policy,
- a filesystem copy worker must not become the owner of file-domain policy,
- a background worker may perform the work, but the service remains the architectural owner.

This distinction keeps cooperative lifecycle logic, long-running work, and ownership boundaries stable even as the system grows.

---

## Early-Boot Integration Rule

A feature that must run very early in boot does **not** automatically become core.

Instead, the allowed model is:

- the feature remains implemented as a **service**,
- core boot explicitly calls a **boot-phase service API**.

### Allowed Pattern

```text
BootInit() -> SomeServiceBootHook()
```

### Disallowed Pattern

```text
BootInit() -> some module lifecycle entry
```

Rationale:

- Boot may call a service directly when early initialization is necessary.
- Boot must not become dependent on module orchestration for critical early-phase work.
- Modules remain runtime lifecycle participants, not boot prerequisites.

This rule is foundational for:

- RTC boot synchronization,
- early device identity reads,
- early secure-element checks,
- early boot-condition inspection,
- future controlled early-init service hooks.

---

## Module Design Rule for Large Driver Populations

HexaOS may eventually contain very large numbers of device drivers across I2C, SPI, UART, RS485, CSI, and other interfaces.

This does **not** justify one module per driver.

Modules must be defined by **runtime domain**, not by raw bus count or device count.

### Correct examples

- `mod_i2c_sensors`
- `mod_polling`
- `mod_web`
- `mod_mqtt`
- `mod_lvgl`
- `mod_connectivity`

### Incorrect examples

- `mod_ds3232`
- `mod_hdc2010`
- `mod_ina3221`
- `mod_st7789`

A bus name may appear in a module only when it also represents a real runtime domain.

For example:

- `mod_i2c_sensors` is valid because it describes a coherent runtime domain.
- `mod_i2c_everything` is invalid because bus affinity alone is not a domain.

---

## Canonical Classification of Future Subsystems

## Webserver

Classification:

- module: yes
- service: yes
- handler: possibly for internal web domains
- adapter: yes, if an HTTP backend wrapper exists
- core: no
- driver: no

Model:

- a web module hosts the lifecycle,
- web services own routes and composed workflow,
- internal web handlers may own sessions, assets, or terminal policy,
- the transport/backend boundary belongs in adapters.

---

## Sensors and Sensor Families

Classification:

- drivers: yes
- adapters: yes
- services: yes
- modules: domain-scoped only
- core: no

Model:

- individual sensors are drivers,
- buses are adapters,
- data collection belongs to services,
- runtime polling belongs to domain modules when needed,
- all outputs should converge through the internal datapoint/telemetry domain rather than each consumer reading drivers directly.

---

## Displays, Touch, and GUI Frameworks

Classification:

- drivers: yes
- adapters: yes
- services: yes
- modules: yes
- core: no

Model:

- display controllers and touch controllers are drivers,
- bus integrations are adapters,
- GUI integration is a service layer,
- display/GUI lifecycle belongs to a module.

A GUI framework such as LVGL is not itself a driver.

---

## OTA Update

Classification:

- service: yes
- module: yes, only when a dedicated runtime domain lifecycle is justified
- adapter: yes
- handler: optional if an update domain owner is justified
- core: no

Core must remain OTA-compatible, but OTA workflow does not become core.

OTA is a canonical example of long-running transactional processing:

- the owning OTA service defines update policy,
- request/upload surfaces submit work to that service,
- chunked write/verify/finalize work may be executed incrementally or by an RTOS worker,
- background execution must not transfer workflow ownership away from the service.

A dedicated OTA module is justified only when the update domain truly needs persistent cooperative lifecycle behavior. One-shot or externally triggered update workflows do not require a module merely because they exist.

---

## MQTT, External API, Home Assistant, Telemetry Upload

Classification:

- modules: usually yes for persistent clients
- services: yes
- adapters: yes
- core: no

Model:

- long-lived client protocol stacks usually justify module lifecycle,
- workflow logic belongs to services,
- backend/network/resource boundaries belong to adapters,
- integration layers such as Home Assistant discovery remain services.

---

## Polling, Modbus, Template-Based Acquisition

Classification:

- protocol/device access: driver
- bus access: adapter
- polling and template execution: service
- domain lifecycle: module when active
- core: no

A polling engine must not be collapsed into a protocol driver.

The protocol driver answers **how to talk**.
The polling service answers **when**, **what**, **how often**, and **where results go**.

---

## Internal Datapoint / Telemetry Database

Classification:

- service: yes
- handler: yes
- module: no by default
- core: no
- driver: no

This domain is expected to be central.

It should own concepts such as:

- datapoint registry,
- current values,
- timestamps,
- units,
- readability/writability,
- change notifications,
- metadata for UI, MQTT, APIs, and automation,
- relationships between sensors, metrics, and actuators.

This domain should become the canonical integration surface for:

- web,
- console,
- MQTT,
- history,
- automation,
- polling,
- and device drivers.

---

## Time, RTC, and NTP Synchronization Example

The time domain is a canonical example of correct layer separation.

## Ownership Model

### Core

Core owns the canonical system time.

Core time functions are the source of truth for:

- current system time,
- time source state,
- time metadata returned to other layers.

### Driver

An RTC chip driver owns only the device implementation.

For example:

- initialize the RTC chip,
- read its registers,
- write its registers,
- validate device-level data.

The RTC driver must not own NTP policy or global time-source selection.

### Adapter

The I2C adapter owns raw bus communication.

### Service

A time synchronization service owns:

- RTC boot read policy,
- NTP apply policy,
- source priority,
- write-back from NTP/system time to RTC,
- retry and scheduling rules when applicable.

### Module

A dedicated module is required only if time synchronization becomes an active runtime domain with independent periodic lifecycle needs.

For a one-shot or mostly passive synchronization model, no dedicated module is required.

---

## Canonical Time Sync Graph

```text
+--------------------------------------------------------------+
|                        system/core/time                      |
|  TimeSetFromRtc()   TimeSetFromNtp()   TimeGetInfo()         |
|  canonical system time source of truth                       |
+-----------------------------^--------------------------------+
                              |
                              |
                   updates canonical system time
                              |
                              |
+-----------------------------+--------------------------------+
|                services/time_sync_service                    |
|  - boot RTC sync policy                                      |
|  - NTP apply policy                                          |
|  - RTC write-back policy                                     |
|  - source priority                                           |
+-------------------^---------------------------^--------------+
                    |                           |
                    |                           |
                    |                           |
      +-------------+----------+     +----------+-------------+
      | drivers/rtc_ds3232     |     | any NTP-capable        |
      | read/write RTC device  |     | network or time source |
      +-------------^----------+     +----------^-------------+
                    |                           |
                    |                           |
       +------------+-----------+               |
       | adapters/i2c_adapter   |               |
       | raw bus access         |               |
       +------------------------+               |
                                                |
                                                v
                                     TimeSyncApplyNtp(...)
```

---

## Canonical Boot Flow for RTC-Assisted Time Initialization

```text
setup()
  -> BootInit()
     -> TimeInit()
     -> ConfigInit()
     -> ConfigLoad()
     -> ConfigApply()
     -> TimeSyncBootTryRtc()
        -> Ds3232Init(...)
        -> Ds3232ReadUnixSeconds(...)
        -> TimeSetFromRtc(...)
     -> continue boot
  -> ModuleInitAll()
  -> ModuleStartAll()
```

Key rule:

- Boot calls the **service boot hook**, not a module.
- The RTC driver is used by the service.
- Core time remains the only owner of canonical system time.

---

## Canonical Runtime Flow for NTP -> System Time -> RTC Writeback

```text
runtime service obtains NTP time
  -> TimeSyncApplyNtp(ntp_unix)
     -> TimeSetFromNtp(ntp_unix)
     -> if RTC write-back is enabled:
          -> Ds3232WriteUnixSeconds(ntp_unix)
```

This flow avoids several architectural violations:

- the RTC driver does not become a policy owner,
- NTP code does not bypass core time,
- web or console layers do not talk directly to the RTC driver,
- boot does not depend on module lifecycle.

---

## Practical Time Sync Function Direction

A canonical function direction may look like this:

```text
BootInit()
  -> TimeSyncBootTryRtc()
     -> Ds3232ReadUnixSeconds()
        -> I2cAdapterWriteRead(...)
     -> TimeSetFromRtc(...)

SomeNtpCapableService()
  -> TimeSyncApplyNtp(...)
     -> TimeSetFromNtp(...)
     -> Ds3232WriteUnixSeconds(...)
        -> I2cAdapterWrite(...)
```

This is the approved pattern for early RTC integration and later NTP convergence.

---

## Interface Exposure Rule

User-facing interfaces must read system-facing state from the correct owner layer.

Examples:

- Web UI should query time information from core time APIs or approved services.
- Console commands should call service APIs or core APIs, not raw device drivers.
- Runtime services should publish datapoints through the datapoint/telemetry domain rather than expose ad-hoc private state.

This keeps drivers private and user surfaces stable.

---

## Filesystem and Storage Principle

Filesystem and persistence integrations must follow the same ownership rules:

- adapters bridge concrete storage backends,
- handlers own Files or persistence domain policy,
- services compose higher workflows such as asset serving, history, export, or migrations,
- core only hosts foundational persistence services that are part of the permanent skeleton.

---

## Long-Term Stability Rule

This document defines the architectural law of HexaOS.

It is expected that:

- file counts will grow,
- features will grow,
- directories will gain subtrees,
- services and drivers will multiply,
- individual implementations will evolve.

What must remain stable is not the exact source tree snapshot, but the **ownership model** and the **dependency rules** defined here.

Whenever a future component is introduced, the correct question is:

> Which layer owns this concern according to the architecture of HexaOS?

If that question is answered correctly, HexaOS remains coherent regardless of scale.
