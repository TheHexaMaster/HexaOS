# HexaOS Architecture

## Purpose

This document defines the architectural model of **HexaOS** and explains how the source tree is organized, how responsibilities are split, and how new code should be placed.

HexaOS is designed to be:

* simple,
* explicit,
* modular,
* easy to audit,
* easy to extend without turning into a framework maze.

The project intentionally avoids unnecessary abstraction and prefers clear ownership, stable boundaries, and predictable execution flow.

---

## Architectural Philosophy

HexaOS is structured around a small number of clearly defined layers.

The core idea is:

* **Core** provides the permanent system foundation.
* **Adapters** connect HexaOS to external APIs, frameworks, SDKs, or libraries.
* **Handlers** own internal system domains and policies.
* **Modules** are runtime participants with lifecycle hooks.
* **Commands** expose operator-facing control surfaces.
* **Headers** define the public and internal interfaces used by the rest of the source tree.

The architecture is intentionally not class-heavy. It is designed around explicit C/C++ translation units, clear ownership, and narrow interfaces.

---

## Design Goals

The source tree should make the following questions easy to answer:

* Where does this code belong?
* Who owns this state?
* Is this code part of the system core, a domain service, a backend bridge, or a module?
* What is public API and what is internal implementation detail?
* What can depend on what?

If the answer is not obvious, the structure is wrong.

---

## Layer Definitions

### 1. Core

`system/core/`

Core contains the permanent foundation of HexaOS. These pieces are always central to the system and should remain conceptually stable over time.

Core is responsible for things such as:

* boot orchestration,
* runtime state,
* logging,
* panic handling,
* RTOS abstraction entry points,
* module registration and lifecycle dispatch.

Core code should not contain domain-specific business logic.

Examples:

* `boot.*`
* `runtime.*`
* `log.*`
* `panic.*`
* `rtos.*`
* `module_registry.*`

**Rule:** if a component is fundamental to the existence of HexaOS itself, it belongs to Core.

---

### 2. Adapter

`system/adapters/`

An adapter is the boundary layer between HexaOS and something external.

That external side may be:

* Arduino APIs,
* ESP-IDF APIs,
* FreeRTOS,
* NVS,
* LittleFS,
* Wi-Fi stack,
* OTA backend,
* USB serial,
* third-party libraries.

An adapter should translate external implementation details into a small and stable internal interface.

Adapters should be:

* narrow,
* backend-aware,
* implementation-specific,
* as non-magical as possible.

Adapters should **not** own high-level system policy.

Examples:

* `console_adapter.*`
* `nvs_adapter.*`
* `rtos_adapter.*`

**Rule:** if a file mostly exists to bridge HexaOS to external APIs or libraries, it is an adapter.

---

### 3. Handler

`system/handlers/`

A handler owns an internal system domain.

A handler is where HexaOS keeps policy, validation, internal state management, domain semantics, and stable internal API for that domain.

A handler may use one or more adapters underneath, but it is not itself a low-level backend bridge.

Typical handler responsibilities:

* domain state ownership,
* validation,
* policy decisions,
* lifecycle-independent service logic,
* stable functions used by commands, modules, or core.

Examples:

* config handler,
* state handler,
* files handler.

**Rule:** if a component owns the internal logic of a system domain, it is a handler.

---

### 4. Module

`system/modules/`

A module is a runtime participant registered into the HexaOS lifecycle.

Modules are the units that participate in:

* init,
* start,
* loop,
* every-100ms hooks,
* every-second hooks.

Modules may depend on core, handlers, and adapters, but they are not the correct place to store central domain logic if that logic must exist independently of the module lifecycle.

Examples:

* `mod_system.*`
* `mod_console.*`
* `mod_storage.*`
* `mod_web.*`
* `mod_lvgl.*`
* `mod_berry.*`

**Rule:** if a component participates in runtime lifecycle scheduling, it is a module.

---

### 5. Commands

`system/commands/`

Commands provide operator-facing or user-facing control entry points.

This layer should do parsing, dispatch, and controlled invocation of core or handler APIs.

Commands should not become the place where domain logic lives.

Examples:

* `command_engine.*`
* `command_builtin.*`

**Rule:** command code should orchestrate, not own the system model.

---

### 6. Headers

`headers/`

Headers define interfaces and shared definitions.

This area should distinguish between:

* public system-facing headers,
* internal schema or meta-expansion helpers,
* build-time definitions.

Headers should remain readable and intentional. If a header mixes too many domains, it should be split.

Recommended categories:

* public API headers,
* shared type/schema headers,
* internal meta headers,
* build configuration headers.

---

## Source Tree Overview

A clean target structure looks like this:

```text
src/
  headers/
    hx_build.h
    hx_schema.h
    hx_config.h
    hx_state.h
    hx_files.h
    internal/
      build_pre.h
      build_post.h
      schema_pre.h
      schema_post.h

  system/
    core/
      boot.*
      log.*
      panic.*
      runtime.*
      rtos.*
      module_registry.*

    adapters/
      console_adapter.*
      nvs_adapter.*
      rtos_adapter.*
      littlefs_adapter.*        # optional future split
      wifi_adapter.*            # future
      ota_adapter.*             # future

    handlers/
      nvs_config_handler.*
      nvs_state_handler.*
      files_handler.*           # optional future split

    commands/
      command_engine.*
      command_builtin.*

    modules/
      mod_system.*
      mod_console.*
      mod_storage.*
      mod_web.*
      mod_lvgl.*
      mod_berry.*
```

This is not meant to force unnecessary fragmentation. It is meant to make ownership explicit.

---

## Dependency Direction

The architecture should follow a strict dependency direction.

### Allowed direction

* Core may depend on build/types/internal helpers.
* Adapters may depend on external APIs and internal shared headers.
* Handlers may depend on adapters, core, and shared headers.
* Modules may depend on handlers, adapters, core, and shared headers.
* Commands may depend on handlers, core, and shared headers.

### Discouraged direction

* Core should not depend on modules.
* Adapters should not depend on modules.
* Adapters should not contain domain policy.
* Commands should not become domain owners.
* Modules should not duplicate handler policy.

The dependency graph should trend downward toward implementation detail, not sideways into coupling.

---

## Ownership Rules

HexaOS relies on explicit ownership.

### Domain ownership

Every persistent or runtime domain must have a clear owner.

Examples:

* configuration domain -> config handler,
* runtime state domain -> state handler,
* filesystem domain -> files handler,
* NVS backend access -> NVS adapter,
* console transport bridge -> console adapter.

If two unrelated places both think they own the same domain, the design is wrong.

---

## Public API vs Internal Implementation

HexaOS should keep a strong distinction between:

* what other parts of the system are allowed to call,
* what is only internal implementation detail.

### Public API

Public system APIs should be declared in dedicated headers under `headers/`.

Examples:

* `hx_config.h`
* `hx_state.h`
* `hx_files.h`

### Internal implementation

Implementation-specific helpers should stay local to the corresponding `.cpp` file or private layer-local headers.

**Rule:** do not leak implementation structure when a narrow public API is enough.

---

## What an Adapter Must Not Become

An adapter must not become:

* a domain owner,
* a policy engine,
* a scheduler,
* a broker for unrelated services,
* a place where cross-domain business rules accumulate.

Bad adapter smell:

* it knows too much about multiple internal subsystems,
* it decides high-level behavior,
* it coordinates unrelated domains,
* it becomes a hidden service layer.

When that happens, the code probably belongs in a handler or a core service instead.

---

## What a Handler Must Not Become

A handler must not become:

* a raw SDK wrapper,
* a build glue file,
* a module lifecycle dispatcher,
* a hardware abstraction layer for an external library.

Bad handler smell:

* most of the code is direct calls into one external backend,
* it exposes backend-specific semantics everywhere,
* it contains almost no real domain policy,
* it behaves like a thin driver bridge.

When that happens, the code is probably an adapter, not a handler.

---

## Current Practical Interpretation in HexaOS

The current HexaOS direction is already good and should be preserved.

### Good current patterns

* `console_adapter` is a real adapter.
* `nvs_adapter` is a real adapter.
* `rtos_adapter` is a real adapter.
* `rtos` belongs in core.
* `module_registry` belongs in core.
* `mod_*` files are correctly treated as modules.
* `command_*` files are correctly treated as commands.

### Current area that may later be split

The current filesystem layer is functionally useful, but it may evolve into a cleaner two-part structure:

* `littlefs_adapter.*`
* `files_handler.*`

This split is not mandatory immediately, but it is the cleaner long-term model if the filesystem domain grows.

---

## Naming Rules

Naming should reflect role, not implementation accident.

### Use `*_adapter` when the file:

* primarily bridges to Arduino, ESP-IDF, FreeRTOS, LittleFS, NVS, Wi-Fi, OTA, or any external library,
* exists mainly to translate backend APIs into HexaOS-facing calls.

### Use `*_handler` when the file:

* owns a HexaOS domain,
* stores policy or validation,
* manages domain semantics,
* provides stable system-facing functions for that domain.

### Use `mod_*` when the file:

* registers with the module lifecycle,
* participates in runtime scheduling.

### Use `command_*` when the file:

* parses and dispatches operator-facing commands.

### Use `hx_*` for public shared headers

This keeps the public surface recognizable and consistent.

---

## Build and Meta Headers

Build helpers and schema expansion helpers should not be confused with public API.

These should ideally live under a clearly internal area such as:

* `headers/internal/`
* or `headers/meta/`

Examples:

* build pre/post include helpers,
* schema expansion helpers,
* internal X-macro support files.

These files are part of the build or code-generation model of the firmware, not part of the public domain API.

---

## Include Discipline

HexaOS should prefer explicit dependencies.

Recommended rule for each `.cpp` file:

1. include its own matching header first,
2. include only the specific headers it truly depends on,
3. avoid relying on a large umbrella header everywhere.

Umbrella headers may still exist for convenience, but they should not hide architectural dependencies.

If a file includes one giant header and nothing else, dependency clarity is lost.

---

## How to Decide Where New Code Belongs

When adding a new component, ask these questions in order.

### Question 1

Does this file mostly talk to an external SDK, framework, library, or hardware-facing API?

* If yes -> **Adapter**.

### Question 2

Does this file own internal HexaOS policy or domain semantics?

* If yes -> **Handler**.

### Question 3

Does this file need module lifecycle hooks such as init/start/loop/timers?

* If yes -> **Module**.

### Question 4

Does this file exist mainly to parse user or operator input and call internal APIs?

* If yes -> **Commands**.

### Question 5

Is this part of the permanent system foundation required regardless of optional modules?

* If yes -> **Core**.

If the answer is still unclear, the responsibility is probably not yet well defined.

---

## Practical Examples

### Example: Wi-Fi integration

* ESP-IDF / Arduino Wi-Fi bridge -> `wifi_adapter.*`
* HexaOS Wi-Fi policy, state ownership, reconnect rules -> `wifi_handler.*`
* runtime lifecycle participant -> `mod_wifi.*` if needed

### Example: OTA

* ESP-IDF OTA backend calls -> `ota_adapter.*`
* update policy, version checks, state transitions -> `ota_handler.*`
* background runtime integration -> `mod_ota.*` if needed

### Example: Filesystem

* direct LittleFS backend access -> `littlefs_adapter.*`
* file-domain API, path rules, atomic write policy -> `files_handler.*`
* storage-related periodic logic -> `mod_storage.*`

### Example: Console

* serial or USB transport bridge -> `console_adapter.*`
* command interpreter usage -> `command_engine.*`
* runtime console module -> `mod_console.*`

---

## Architectural Anti-Patterns

The following patterns should be avoided.

### 1. Hybrid files that own too many layers at once

A file should not simultaneously be:

* external backend bridge,
* domain owner,
* scheduler,
* module,
* and command surface.

### 2. Hidden policy inside adapters

If a backend bridge decides high-level behavior, it is no longer a clean adapter.

### 3. Domain logic inside commands

Commands should trigger actions, not become the place where system semantics are implemented.

### 4. Public API scattered without pattern

Public APIs should be easy to find and consistent in placement.

### 5. Global umbrella includes everywhere

Convenient in the short term, but harmful to dependency clarity over time.

---

## Recommended Direction for HexaOS

The current architectural direction is correct and should be continued.

Recommended improvements over time:

1. keep the current layer model,
2. preserve the adapter definition as the external boundary layer,
3. preserve handlers as domain owners,
4. keep core small and permanent,
5. keep modules lifecycle-focused,
6. unify placement of public API headers,
7. split hybrid layers when they become too mixed,
8. keep include discipline strict.

This will keep HexaOS understandable even as the codebase grows.

---

## Final Summary

HexaOS should be understood as a layered embedded operating environment with explicit ownership boundaries.

The correct mental model is:

* **Core** = permanent system foundation
* **Adapter** = bridge to the outside world
* **Handler** = internal owner of a system domain
* **Module** = lifecycle participant
* **Commands** = operator-facing control surface
* **Headers** = intentional interface definition

The architecture should remain simple, explicit, and resistant to accidental layer mixing.

The goal is not abstraction for its own sake.
The goal is a codebase where the role of each file is obvious, stable, and defensible.
