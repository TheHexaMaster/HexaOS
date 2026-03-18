# HexaOS

**HexaOS** is an open-source operating system and development platform for ESP32-class microcontrollers, designed to serve as a practical foundation for embedded applications, automation devices, local user interfaces, scripting, recovery workflows, and long-term product-oriented firmware.

The project is built around a simple idea: embedded software should be powerful, modular, and maintainable **without turning into an over-engineered C++ framework**.

## Vision

HexaOS aims to become a universal and extensible ESP32 software platform that can be used for:

- automation and smart-home devices
- industrial and energy monitoring gateways
- local display-driven control panels
- scriptable embedded systems
- educational and experimental MCU projects
- long-term maintainable custom firmware products

The long-term ambition is to provide a clean system foundation that can scale from small headless devices up to richer display-equipped products, while remaining understandable to normal programmers and makers.

## Core design philosophy

HexaOS is intentionally designed around the following principles:

- **Simplicity first** – if something can be implemented clearly, it should not be made more abstract than necessary.
- **Readable architecture** – source code should stay easy to navigate, reason about, and extend.
- **Modularity without framework bloat** – the system is split into modules and services, but avoids unnecessary C++ hyper-abstractions.
- **Practical embedded engineering** – the goal is not academic purity, but a firmware platform that works well on real hardware.
- **Hybrid use of ESP-IDF and Arduino** – HexaOS is developed in an Arduino-style environment, while still taking advantage of modern ESP-IDF capabilities underneath.

## Project structure

At the project level, HexaOS is organized into a small number of clearly defined areas:

- `docs` – project documentation
- `environment/boards` – board definitions used during build
- `environment/boards/variants` – `pins_arduino.h` files for specific hardware variants
- `environment/envs` – separated build environment snippets
- `environment/partitions` – partition layouts for different flash sizes
- `environment/scripts` – pre-build and post-build helper scripts
- `include` – shared build/system headers and main HexaOS headers
- `lib` – external libraries only
- `src` – internal HexaOS source code

## Tooling and build model

HexaOS is built with **pioarduino** in an **Arduino environment**, but the project is intentionally used in a **hybrid mode** with custom SDK configuration so it can benefit from both modern **ESP-IDF** functionality and Arduino libraries.

This makes HexaOS suitable for developers who want the convenience of Arduino-style development without giving up the lower-level capabilities of the ESP32 software stack.

## Architecture direction

HexaOS is being shaped as a lightweight system with clearly separated layers such as:

- core boot/runtime services
- platform-specific ESP32 functionality
- persistent configuration and state services
- filesystem and recovery support
- optional modules such as console, web UI, scripting, and LVGL-based display support

The system takes inspiration from proven ideas used in Tasmota, but it is **not intended to be a simple rebrand or direct fork**. The goal is to build a cleaner and more extensible architecture from the ground up, while still borrowing good practical concepts where it makes sense.

## Flash and persistence model

HexaOS uses a partition strategy shared across **4 MiB** and **16 MiB** flash variants, while **1 MiB devices are no longer a target**.

The planned flash model includes:

- `otadata` – OTA boot metadata
- `nvs_factory` – optional per-device factory/manufacturing data
- `nvs` – runtime configuration overrides
- `nvs_state` – persistent runtime state
- `safeboot` – dedicated recovery image
- `app0` – main HexaOS application image
- `littlefs` – user filesystem for scripts, assets, and runtime files

This layout is designed to support reliable OTA updates, recovery workflows, runtime persistence, and user-extensible filesystems in a way that remains practical for real devices.

## Why HexaOS exists

HexaOS started from the need for a cleaner, more capable, and more future-oriented embedded platform than a heavily modified legacy firmware base.

Instead of endlessly patching an existing project, HexaOS is intended to become a purpose-built system with:

- a clearer architecture
- better long-term maintainability
- stronger recovery and update strategy
- cleaner module boundaries
- room for modern UI, scripting, and storage concepts

## Current status

HexaOS is currently in its early architectural stage. The foundation is being built first:

- core boot and runtime structure
- module system
- persistent storage model
- partition strategy
- internal services layout

This early phase is focused on creating a solid and understandable base before larger features are layered on top.

## Long-term goals

In the long run, HexaOS aims to become:

- a serious open-source ESP32 operating platform
- a reusable base for custom devices and products
- a practical development platform for advanced embedded projects
- a clean bridge between low-level embedded power and approachable application development

## 📄 Licenses

This project is released under [GPL-3.0 license](./LICENSE)
