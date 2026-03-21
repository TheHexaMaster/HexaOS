# HexaOS

**HexaOS** is an open-source embedded operating platform for **ESP32-class microcontrollers**, built for developers who want a **clean, long-term maintainable firmware architecture** without turning the project into a bloated framework.

It is designed as a practical system foundation for:

- automation and smart-home devices
- industrial and energy-monitoring gateways
- local display-driven control panels
- scriptable embedded products
- protocol bridges and data concentrators
- long-lived custom firmware platforms

HexaOS is not trying to imitate desktop operating systems. It is an **embedded runtime architecture** focused on clear ownership, explicit control flow, recoverability, and product-grade extensibility.

---

## ✨ Why HexaOS exists

HexaOS exists because real embedded products eventually outgrow ad-hoc firmware.

At some point, a serious device needs more than just:

- a main loop with scattered globals,
- a pile of device-specific code,
- random SDK calls leaking into every subsystem,
- or a legacy codebase patched beyond readability.

HexaOS is the answer to that problem.

The project is built around a simple idea:

> embedded software should be **powerful, modular, and auditable** without becoming an over-engineered C++ framework.

---

## 🧭 Design principles

HexaOS is intentionally shaped by a few permanent principles:

- **Simplicity first** — no abstraction without real benefit.
- **Clear ownership** — every responsibility must belong to the correct architectural layer.
- **Readable source tree** — code should stay navigable even as the project grows.
- **Explicit control flow** — no hidden magic, no opaque framework behavior.
- **Modularity without fragmentation** — features can grow without turning the tree into chaos.
- **Practical embedded engineering** — recovery, persistence, hardware access, and runtime control matter more than theoretical purity.
- **Long-term maintainability** — the architecture should still make sense years later, even after large growth in features and code size.

---

## 🏗️ Architectural model

HexaOS is organized around **clear layer ownership**, not around deep inheritance trees or framework-style inversion.

### Permanent architectural layers

- **Core** — permanent system skeleton and foundational runtime services
- **Adapters** — bridges to ESP-IDF, Arduino, buses, storage backends, and external APIs
- **Drivers** — reusable device and protocol implementations
- **Handlers** — internal HexaOS domain owners with stable internal APIs and policy
- **Services** — composed runtime behavior built above handlers, drivers, and adapters
- **Modules** — top-level lifecycle orchestrators for runtime domains
- **Commands** — frontend-agnostic command execution layer shared by multiple user surfaces

### Key architectural rules

- **Core is not a feature layer.**
- **Modules are domain lifecycle orchestrators, not one-per-driver wrappers.**
- **Services may exist with or without modules.**
- **Early-boot work may call service boot hooks directly, without turning that service into Core.**
- **Build-disabled features are architecturally absent** — not just runtime-inactive.
- **Module order is not a dependency-resolution mechanism.**

For the full long-term architectural model, see [ARCHITECTURE.md](./ARCHITECTURE.md). 

---

## 🧩 What HexaOS is designed to host

HexaOS is being built as a foundation for systems such as:

- local interactive console control
- web configuration and diagnostics
- Berry scripting and future developer tooling
- display and touch subsystems
- storage and recovery workflows
- telemetry publishing and external integrations
- sensor acquisition and protocol polling
- internal datapoint / telemetry services
- OTA update pipelines
- future multi-endpoint user surfaces

The intent is not to hard-code one product type, but to provide a stable architecture for many embedded product profiles.

---

## 🧱 Repository structure

At repository level, HexaOS is intentionally kept simple:

- `docs/` — project documentation
- `environment/boards/` — board definitions used during build
- `environment/boards/variants/` — `pins_arduino.h` files for hardware variants
- `environment/envs/` — separated build environment snippets
- `environment/partitions/` — partition layouts for different flash sizes
- `environment/scripts/` — build helper scripts
- `include/` — shared build/system headers and public HexaOS-facing headers
- `lib/` — external libraries only
- `src/` — internal HexaOS source code

Inside `src/system/`, the long-term layer model is:

- `core/`
- `adapters/`
- `drivers/`
- `handlers/`
- `services/`
- `commands/`
- `modules/`

This structure is meant to scale without losing clarity. The goal is not to create more folders, but to preserve ownership boundaries.

---

## ⚙️ Tooling and build model

HexaOS is built with **pioarduino** in an **Arduino environment**, while intentionally operating in a **hybrid mode** that also takes advantage of modern **ESP-IDF** capabilities.

This gives the project two important properties:

- the convenience and ecosystem reach of Arduino-style development
- access to lower-level platform capabilities needed for serious ESP32 products

HexaOS is not meant to be trapped in either extreme:
- not a “just Arduino sketch” project
- not an overcomplicated IDF-only framework

It is a practical embedded platform built to use both where each makes sense.

---

## 🧠 Runtime philosophy

HexaOS uses a **thin top-level execution bridge** with explicit boot and cooperative runtime dispatch.

At a high level, the permanent shape is:

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

This keeps the top-level runtime readable while allowing larger runtime domains to participate through modules and services. The long-term model also explicitly allows **early-boot service hooks** when something must happen before normal runtime orchestration begins.

---

## 💾 Persistence, recovery, and flash model

HexaOS targets a partition model designed for reliable embedded products rather than toy firmware images.

The flash strategy is centered around concepts such as:

- OTA boot metadata
- persistent configuration overrides
- persistent runtime state
- dedicated recovery / safeboot support
- primary application image
- user filesystem for scripts, assets, and runtime files

The project is designed around practical persistence and recovery workflows rather than assuming a single monolithic firmware image forever. The original repository README already established the 4 MiB / 16 MiB direction and a recovery-oriented persistence model, and that direction remains aligned with the current architecture. fileciteturn14file13

---

## 🔌 Build-time feature model

HexaOS distinguishes between:

- **architecturally core functionality**
- **mandatory functionality for a specific product build**
- **optional runtime features**

These are **not the same thing**.

A feature that is disabled at build time is expected to disappear as a **feature family**, not merely remain dormant at runtime. That means the same build selector philosophy applies across the relevant stack: module shells, services, handlers, adapters, commands, and any dependent integration points.

This keeps the resulting binary smaller, clearer, and more honest about what actually exists in a given product build.

---

## 🖥️ User surfaces

HexaOS is built around the idea that multiple control surfaces should be able to reuse the same internal execution layers.

This includes:

- local console
- web UI
- web terminal
- future Berry terminal / developer tools
- future hosted or remote control surfaces

The command layer exists specifically so that user-facing endpoints do not need to reinvent domain execution logic each time.

---

## 🚀 Current direction

HexaOS has moved beyond a placeholder repository stage and is now driven by a defined architecture with explicit long-term rules for:

- layer ownership
- lifecycle participation
- dependency direction
- early-boot integration
- service and module classification
- build-time feature gating

That architectural direction is now a first-class part of the project, not an afterthought. The current README replaces the original initial-commit era project description with one aligned to the repository’s real long-term design. The initial README was written before the architecture and source tree model were defined. fileciteturn14file11turn14file1

---

## 🛣️ Long-term goals

In the long run, HexaOS aims to become:

- a serious open-source operating platform for ESP32-class devices
- a reusable foundation for custom products and gateways
- a stable base for local UI, scripting, telemetry, and automation
- a practical bridge between low-level embedded control and higher-level product features
- a firmware architecture that remains understandable even after large long-term growth

---

## 📚 Documentation

- [ARCHITECTURE.md](./ARCHITECTURE.md) — long-term architectural whitepaper
- [LICENSE](./LICENSE) — project license

As the project grows, additional design and implementation documents will live under `docs/`.

---

## 📄 License

This project is released under the [GPL-3.0-only license](./LICENSE).
