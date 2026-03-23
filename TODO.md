## Until VER 0.0.2 release (allready done)

- [x] Header system and build / config schema refactor
- [x] Separate console and log system (done 19.03.2026)
- [x] Command engine separated from console to custom layer (preparation for future extensions - web console, telnet etc)
- [x] STATES - We need to finish engine to be ready to work with internal / externally defined states (modules, drivers, user-defined scripted states from scripting engine / console engine in future etc)
- [x] STATES - Console command handling for states, updated handler for increment / decrement for integers, toggle for booleans
- [x] STATES - Console / module persistence question to solve
- [x] STATES - Cashing handler / save delay, settable in config settings (to protect NVS from too-fast write cycles, suggested minimum - 1s). Per NVS.
- [x] STATES - Added DEBUG logs in new logging format
- [x] STATES - Add state format (total NVS delete to factory state) and state info commands (NVS statistics - space, filled, operations etc)
- [x] STATES - Define write restriction flag for build-generated states. If true, user / runtime EXCEPT system self cannot change the value. 
- [x] STATES - Refactor ownership system - no strings, only enums and different logic.
- [x] STATES - Bugfix - delay shall apply only on value writes, not on creation / deletion / format / another commands. 
- [x] STATES - Bugfix - catalog data inconsistency 
- [x] FACTORY NVS - Because of lack of use, this NVS will be deleted and not supported in later models. 
- [x] STATES and CONFIGS - added FLOAT variable definition + new TYPE SCHEME MACRO for different variables at build (XS,XI,XB,XF)
- [x] STATES - Stable and tested release.
- [x] CONFIG - Refactor to pre-final logic and test.
- [x] LITTLEFS - Extend adapter with complex funcs to manage FS - initial commit. Need deep refactor, test and optim.
- [x] RTOS - Create adapter and handler for convience async handling with priority and task management covered under HexaOS.
- [x] RTOS - Integration PART 1 - update to existing modules using RTOS external unmanaged calls - log an console
- [x] RTOS - REfactor - move from handler to core without using build selector - native RTOS core implementation. This way core RTOS can manage different RTOS adapters in future. 
- [x] RTOS - Integration PART 2 - update to existing modules using RTOS external unmanaged calls - NVS config and STATE. 
- [x] RTOS - Integration PART 3 - littlefs_handler 
- [x] TIME - We need to create central time engine / interface prepared to operate from RTC and timming for events, synchronysing from internal RTC modules (i2c driver) and NTP sync (prepare, web interface later)
- [ ] RUNTIME - Refactor, strict module declaration and integration, command console to show actual runtime states
- [ ] LOG - Add 5th log level - "LLD", displayng Low Level Debug Messages
- [ ] LOG - Different log levels for different outputs (serial console, web console, terminal etc) - configs + handling
- [ ] LOG - Feature - color / font difference for various log levels / command inputs / outputs 
- [ ] LOG - Replace timming from TICK to real time (00:00:00 from boot, time after sync from external RTC)
- [ ] REFACTOR - Final pre-release code refactor.


# HexaOS 0.0.2 Release TODO Plan

## Purpose

This document defines the **chronological work plan required before releasing HexaOS 0.0.2**.

It is not a copy of the historic repository TODO. It is a **release-oriented architectural plan** derived from:

- the current source tree,
- the current `ARCHITECTURE.md`,
- the current public `README.md`,
- and the historic handwritten `TODO.md`.

Version **0.0.2** should be treated as an **architecture-lock release**.

That means:

- the permanent ownership model must already be reflected in the codebase,
- the build system must already honor architectural feature boundaries,
- the runtime lifecycle must already support the future model,
- and the source tree must already be ready for future services, drivers, and domain modules without requiring another structural rewrite.

The goal of `0.0.2` is **not** to ship all future features.
The goal is to ship a codebase that is **architecturally compatible with future development**.

---

## 0.0.2 Release Definition

HexaOS `0.0.2` should guarantee all of the following:

1. **The code follows the architecture whitepaper, not only the documentation.**
2. **Optional feature families are truly build-gated across the whole stack.**
3. **The lifecycle model supports future active services and domain schedulers.**
4. **The source tree already has canonical places for future services and drivers.**
5. **The next large features (RTC/NTP sync, sensors, networking, web, OTA, LVGL, scripting) can be added without another architectural rewrite.**

---

## Non-Goals for 0.0.2

The following items are **not required as full implementations** for `0.0.2` and should not delay the release unless they are needed as scaffolding for architecture alignment:

- full webserver implementation,
- OTA implementation,
- MQTT / Home Assistant integration,
- Wi-Fi and Ethernet full runtime stack,
- JSON parser,
- low-level memory debugger,
- generic task manager,
- command priorities / async command execution buffer,
- Berry runtime implementation,
- LVGL runtime implementation.

These remain future feature milestones.
For `0.0.2`, only the **architectural foundations required to host them cleanly later** matter.

---

## Release Order

The work below is ordered chronologically. Later items assume earlier items are already complete.

---

## 1. Freeze Build Selector Taxonomy and Feature Gating

**Goal:** make build-time feature control match the architecture.

### Why

The architecture now defines a strict rule:

- a disabled feature is **architecturally absent**,
- not merely idle at runtime.

The current code still contains incomplete gating patterns and feature flags that are not yet backed by real feature families.

### Required work

- [ ] Normalize the naming model in `hx_build.h`.
- [ ] Separate selectors clearly by category:
  - modules,
  - services,
  - handlers,
  - features,
  - backend/adapter selectors where applicable.
- [ ] Remove or disable phantom feature flags that currently advertise features not yet implemented.
- [ ] Add build-time dependency checks for hard requirements.
- [ ] Ensure a disabled feature family is compiled out through the **entire stack**, not only at one entry point.
- [ ] Audit boot includes and boot calls so they are gated consistently.
- [ ] Audit command registration so disabled features do not leave behind dead command surface.
- [ ] Audit module registry so optional domains are not accidentally present by source inclusion alone.

### Deliverable

A clean build selector model where architecture, build flags, and binary contents all mean the same thing.

---

## 2. ✅ Align the Source Tree with the Architectural Layer Model

**Goal:** make the real source tree reflect the permanent whitepaper structure.

### Why

The architecture already defines canonical layers:

- `core/`
- `adapters/`
- `handlers/`
- `drivers/`
- `services/`
- `commands/`
- `modules/`

The current tree is still missing real `drivers/` and `services/` usage.
Without this, future work will either be pushed into the wrong layers or will force another structural migration later.

### Required work

- [ ] Add real `system/services/` and `system/drivers/` directories to the active source tree.
- [ ] Add at least one canonical service skeleton file.
- [ ] Add at least one canonical driver skeleton file.
- [ ] Add minimal header comments / templates for new layer files so future work follows the same style immediately.
- [ ] Update include paths and documentation assumptions where the tree shape changes.

### Deliverable

A real codebase that already contains the permanent architectural layer destinations, not only a document that mentions them.

---

## 3. Solve Board / Pin / Bus Foundation Before New Drivers

**Goal:** create the hardware foundation needed before RTC, sensors, displays, UART devices, networking peripherals, and future boards multiply.

### Why

Future HexaOS growth will depend on:

- I2C devices,
- SPI devices,
- UART / RS485 devices,
- RTC chips,
- displays,
- touch controllers,
- storage peripherals,
- board-specific variants.

If board/pin/bus ownership is not solved early, hardware-specific logic will spread across services and drivers.

### Required work

- [ ] Define the permanent pinout / board mapping mechanism.
- [ ] Decide what belongs in board variant files versus runtime configuration.
- [ ] Introduce canonical adapter boundaries for at least:
  - I2C,
  - SPI,
  - UART.
- [ ] Define how buses are selected and addressed by future drivers.
- [ ] Ensure bus adapters remain adapters, not future policy owners.
- [ ] Define how future services and drivers will resolve bus instances safely.

### Deliverable

A stable hardware access model that future drivers can depend on without ad-hoc board logic.

---

## 4. ✅ Extend the Lifecycle Model with `Every10ms`

**Goal:** bring the code-level lifecycle in line with the now-agreed runtime model.

### Why

The architecture discussion established that:

- `Loop()` alone is too opportunistic,
- `100ms` and `1s` are too coarse for many future active domains,
- but a global `5ms` hook is too aggressive as a default contract.

The chosen fast soft-periodic cadence is:

- `Loop()`
- `Every10ms()`
- `Every100ms()`
- `Every1s()`

The code does not yet expose that lifecycle contract.

### Required work

- [ ] Add `Every10ms()` to `hexaos.cpp` top-level runtime dispatch.
- [ ] Add `SystemEvery10ms()` to core runtime flow.
- [ ] Extend `HxModule` with `every_10ms` callback support.
- [ ] Extend `ModuleRegistry` with `ModuleEvery10ms()`.
- [ ] Update module stubs to the new canonical lifecycle shape.
- [ ] Document that `Every10ms()` is a **soft cadence**, not a hard real-time guarantee.

### Deliverable

A lifecycle model in code that matches the architecture and is ready for future active services.

---

## 5. ✅ Introduce the Shared Domain Scheduler Primitive

**Goal:** avoid both extremes:

- one giant global scheduler,
- or many ad-hoc `millis()` schedulers scattered through modules.

### Why

The architecture now assumes:

- a small global cadence model,
- but domain-owned scheduling for polling-heavy subsystems.

Future modules such as sensor polling, history flushing, connectivity retries, and queue draining will need:

- intervals,
- deadlines,
- phase offsets,
- bus/resource pacing,
- work budgeting.

That should not be hand-rolled repeatedly.

### Required work

- [ ] Define one reusable scheduling primitive for domain use.
- [ ] Keep it out of the architecture as a new top-level layer.
- [ ] Support at minimum:
  - interval,
  - next-due time,
  - enable/disable,
  - phase offset,
  - budgeted execution,
  - optional priority / weight field.
- [ ] Make it suitable for hosting inside services or modules.
- [ ] Do **not** turn it into a system-wide mega scheduler.

### Deliverable

A reusable domain scheduler utility that future services/modules can instantiate without inventing their own timing logic.

---

## 6. ✅ Finish Runtime Introspection and Strict Module Integration

**Goal:** make the running system able to describe itself clearly.

### Why

This is already an open item in the historic TODO and remains architecturally important. The system now has core, optional modules, build selectors, and runtime flags, but still lacks a clean introspection surface. 

### Required work

- [ ] Add strict runtime/module declaration model in the registry.
- [ ] Track at least:
  - build-enabled state,
  - registered state,
  - init result,
  - started state,
  - optional runtime health field.
- [ ] Expose module/runtime status through commands.
- [ ] Add a runtime command namespace (for example `runtime status`, `module list`, `module info`).
- [ ] Keep runtime introspection out of `state` persistence.
- [ ] Keep module ordering out of dependency resolution.

### Deliverable

A runtime introspection surface that lets HexaOS describe its own current architecture and lifecycle state from the console and future frontends.

---

## 7. ✅ Finalize the NVS Boundary as an Audited Hybrid

**Goal:** keep the current audited hybrid model, but remove the remaining wrong ownership.

### Why

The architecture now allows the current `nvs_adapter` as an **audited hybrid exception**, but that does not justify letting it own high-level failure policy. The current code still mixes NVS backend work with failure behavior that belongs higher in the stack.

### Required work

- [ ] Keep the unified `nvs_adapter` model for `0.0.2`.
- [ ] Move fatal failure policy (`panic`, release decision, recovery decision) out of the adapter and into boot/core.
- [ ] Ensure the adapter returns status rather than deciding system fate.
- [ ] Keep adapter comments honest about its hybrid role.
- [ ] Audit naming and exported API so `config` and `state` use the same persistence language consistently.

### Deliverable

A consciously hybrid but still disciplined NVS backend aligned with the architecture exception rule.

---

## 8. ✅ Complete the LittleFS Boundary Refactor

**Goal:** make filesystem ownership follow the architecture.

### Why

The historic TODO already marked LittleFS as only an initial commit that still needs deeper refactor, test, and optimization.

In the current tree, the filesystem logic is still too mixed between domain handling and direct backend use.

### Required work

- [ ] Split raw `FS` / `LittleFS` interaction into a real adapter.
- [ ] Keep the filesystem handler as the Files domain owner.
- [ ] Gate the full flash-FS feature family consistently.
- [ ] Gate boot calls, commands, and future routes through the same feature selectors.
- [ ] Retest mount, unmount, format, read, write, append, stat, list, and atomic write semantics.
- [ ] Confirm thread-safety / RTOS safety after the split.
- [ ] Add or finalize file headers for all touched files.

### Deliverable

A real Files domain with a backend adapter, ready for future web file manager, scripting assets, and OTA-related storage workflows.

---

## 9. ✅ Introduce the First Real Passive Service: Time Synchronization Service

**Goal:** turn the architecture’s timesync model into a real service boundary before RTC and NTP features arrive.

### Why

The architecture already defines the canonical ownership model:

- `core/time` = canonical system time owner,
- RTC driver = device logic,
- time sync service = policy,
- boot = explicit early service hook when needed.

The current tree has `core/time`, but not yet the service layer that future RTC/NTP work should plug into.

### Required work

- [ ] Add `time_sync_service` as the first real passive service.
- [ ] Define boot-phase API for future early RTC read.
- [ ] Define runtime API for future NTP apply / RTC write-back.
- [ ] Keep `core/time` as the sole source of truth.
- [ ] Ensure future RTC drivers cannot bypass core time directly.
- [ ] Ensure future web/console layers will call service/core APIs, not device drivers.

### Deliverable

A concrete service example in the codebase that proves the architecture’s early-boot service-hook rule.

---

## 10. ✅ Split `command_builtin.cpp` Before It Becomes the Next Monolith

**Goal:** keep the command layer maintainable before more command families arrive.

### Why

The current command engine is already separated from the console, which is good and should remain. The next problem is the growth of `command_builtin.cpp`, which is already the wrong place to keep accumulating runtime, file, web, OTA, and future control-plane commands.

### Required work

- [ ] Split the builtin command file by domain.
- [ ] Suggested minimum split:
  - `cmd_help.cpp`
  - `cmd_log.cpp`
  - `cmd_time.cpp`
  - `cmd_config.cpp`
  - `cmd_state.cpp`
  - `cmd_runtime.cpp`
  - optionally `cmd_files.cpp`
- [ ] Keep `command_engine.*` central and frontend-agnostic.
- [ ] Keep registration explicit and deterministic.
- [ ] Do **not** implement command scheduling/priorities yet.

### Deliverable

A command layer that stays clean when web terminal, file commands, and future service commands arrive.

---

## 11. ✅ Add Minimal Files Command Surface

**Goal:** prove the Files domain is usable through the shared command architecture before web UI arrives.

### Why

The historic TODO already mentions filesystem console handling as a future item.

This is a good `0.0.2` task because it validates:

- Files handler,
- future flash-FS feature gating,
- command frontend independence,
- and the future web/file-manager path.

### Required work

- [ ] Add a minimal `files` command namespace.
- [ ] Support at least a safe subset such as:
  - info,
  - list,
  - read text,
  - remove,
  - mkdir/rmdir,
  - stat.
- [ ] Keep writes conservative and explicit.
- [ ] Respect build gating so these commands disappear when flash-FS support is absent.

### Deliverable

A first reusable control-plane surface for filesystem features that future web UI can mirror rather than reinvent.

---

## 12. ✅ Finish the Logging Foundation Needed Before More Endpoints Arrive

**Goal:** complete only the parts of logging that are necessary before multi-endpoint growth.

### Why

The historic TODO still contains several open logging tasks. Some of them are already partially addressed by the current time core and log-stamp path, but the logging model is not fully release-closed yet.

### Required work

- [ ] Add the `LLD` log level as the fifth level.
- [ ] Confirm and finalize the permanent log stamp rule:
  - monotonic uptime before synchronization,
  - wall clock after synchronization.
- [ ] Decide whether log sink routing stays single-sink for `0.0.2` or whether a minimal internal routing table is required.
- [ ] If multi-sink routing is not implemented yet, document that only the foundation is frozen in `0.0.2` and full multi-endpoint routing is deferred.
- [ ] Defer styling/color work unless a second frontend lands in the same release.

### Deliverable

A logging core that is stable enough for future web/UART/remote outputs without forcing a second architectural rewrite later.

---

## 13. Perform the First Real Documentation-to-Code Alignment Pass

**Goal:** make docs and code tell the same story at the `0.0.2` tag.

### Why

By `0.0.2`, HexaOS should stop presenting architecture as a moving intuition and start presenting it as a stable development contract.

### Required work

- [ ] Sync `README.md` with the actual post-`0.0.2` tree and release meaning.
- [ ] Sync `ARCHITECTURE.md` wording where code-level naming differs.
- [ ] Replace any stale comments that still describe pre-refactor ownership.
- [ ] Ensure newly introduced `services/` and `drivers/` appear in docs and tree.
- [ ] Ensure code comments no longer contradict the architecture whitepaper.

### Deliverable

A release where architecture, documentation, and code are aligned enough to guide future contributors safely.

---

## 14. Final Pre-Release Hardening Pass

**Goal:** ship `0.0.2` as a clean architectural baseline rather than as an unfinished refactor branch.

### Required work

- [ ] Full header sweep for all touched files.
- [ ] Final naming sweep for consistency across modules, handlers, services, and adapters.
- [ ] Remove or disable dead build flags and misleading stubs.
- [ ] Run build matrix checks for relevant selector combinations.
- [ ] At minimum test:
  - base build with optional modules disabled,
  - build with LittleFS enabled,
  - build with LittleFS disabled,
  - both console backends if available,
  - command engine initialization with current builtin set,
  - state/config persistence behavior after the refactors.
- [ ] Validate no boot path still depends on accidental module order.
- [ ] Validate no disabled feature family leaks commands, code paths, or link dependencies.

### Deliverable

A `0.0.2` release candidate that is safe to build on instead of immediately needing another cleanup cycle.

---

## Recommended Explicit Deferrals (after 0.0.2)

These should stay **after** `0.0.2` unless they are needed as scaffolding for one of the tasks above:

- full webserver runtime,
- OTA implementation,
- MQTT and Home Assistant integration,
- full Wi-Fi / Ethernet runtime,
- JSON parser,
- low-level raw memory debugger,
- generic RTOS task manager,
- command priorities / execution buffer scheduling,
- Berry runtime,
- LVGL runtime,
- advanced log styling,
- upload/download transaction workers beyond minimal architectural scaffolding.

---

## Final 0.0.2 Exit Criteria

HexaOS `0.0.2` is ready when all of the following are true:

- [ ] The code follows the whitepaper layer model in all currently implemented domains.
- [ ] The codebase contains the canonical destinations for future `services/` and `drivers/`.
- [ ] The top-level lifecycle includes `Every10ms()` and supports future active domains.
- [ ] Optional feature families are fully build-gated across their entire stack.
- [ ] Runtime introspection exists and can describe module/runtime status cleanly.
- [ ] The NVS boundary is disciplined enough to remain the audited hybrid exception.
- [ ] The filesystem stack follows adapter/handler ownership correctly.
- [ ] The first real passive service exists and proves the early-boot service-hook model.
- [ ] The command layer is no longer centered around one growing monolith file.
- [ ] Documentation and source code tell the same architectural story.

## Future versions todo by priority

- COMMANDS - Refactor and prototype of central command register and execution buffer (callbacks, scheduling, priorites, etc) (maybe RTOS adapter / handler first?)
- CONSOLE - Now we have defined only SERIAL console trought HWCDC / JTAG build selector. Need to add typical SERIAL UART as a build selector option (possible serial fallback as optional setting from config with default false.)
- JSON parsing implementation
- Solve pinout definition mechanism
- filesystem console data handler
- Wifi / Ethernet implementation
- Webserver (start of development)
- OTA handling
- DEBUG - We need console based LOW-LEVEL debugger, capable to call introspect <read/write> <iram/psram/flash/pointer> at selected <hex address> and return/write selected bytes <1,2,4,8,16,32,64...>. Return bytes in HEX. It shall be RAW debugger without any protection (in debug mode, crashes are acceptable.)
- TASK Manager (RTOS-powered)