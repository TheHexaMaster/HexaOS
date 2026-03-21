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
| boot | system loop | runtime | time | log | panic |          |
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
| bridges to ESP-IDF / Arduino / libraries / buses / backends  |
+------------------------------+-------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|               Hardware / SDK / External Backends              |
| I2C | SPI | UART | FS | NVS | Wi-Fi | Ethernet | HTTP | etc. |
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

---

## 6. Module

A module is a **top-level lifecycle orchestrator**.

A module participates in one or more of:

- init,
- start,
- loop,
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
system/drivers/rtc/
system/drivers/sensors/
system/drivers/display/
system/services/web/
system/services/telemetry/
system/services/polling/
```

---

## Dependency Direction Rules

### Allowed

- Core may depend on shared headers and other core components.
- Handlers may depend on core and adapters.
- Drivers may depend on adapters and shared headers.
- Services may depend on core, handlers, drivers, and adapters.
- Modules may depend on core, handlers, services, and drivers.
- Commands may depend on core, handlers, and services.

### Forbidden or Strongly Discouraged

- Core depending on optional feature modules.
- Adapters owning domain policy.
- Handlers becoming raw SDK wrappers.
- Drivers owning user-facing workflow policy.
- Modules created per device without real lifecycle need.
- Commands becoming persistence or domain owners.
- UI transports bypassing command/service/domain boundaries when those already exist.

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
  -> Every100ms dispatch
  -> EverySecond dispatch
```

Interpretation:

- `hexaos.cpp` remains a thin entry bridge.
- Boot orchestration belongs to core.
- Mandatory runtime housekeeping belongs to core.
- Optional or domain-scoped runtime participants belong to modules.

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
- module: yes, if periodic or stateful runtime behavior exists
- adapter: yes
- handler: optional if an update domain owner is justified
- core: no

Core must remain OTA-compatible, but OTA workflow does not become core.

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
