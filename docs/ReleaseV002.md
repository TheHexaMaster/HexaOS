# HexaOS v0.0.2 — Architecture-Lock Release

**Release date:** 2026-03-23
**Target platform:** ESP32 family (all variants)
**License:** GPL-3.0-only

---

## What is this release

`v0.0.2` is the **architecture-lock release** of HexaOS.

It does not ship every planned feature. Its purpose is to establish a codebase that is structurally stable enough to support all future development without requiring another architectural rewrite. Every major subsystem present in this release follows the permanent ownership model defined in `ARCHITECTURE.md`.

From this point forward, the layer model, dependency direction rules, build-time feature gating model, and lifecycle contract are frozen as the development foundation.

---

## What is working in this release

### Boot orchestration

The boot sequence is explicit and fully deterministic:

1. RTOS primitives initialized
2. Logging backend started
3. User interface transport attached
4. Monotonic time initialized
5. Boot banner printed with system name, version, board target, reset reason, chip model, revision, core count and flash size
6. Configuration loaded from NVS and applied
7. Pinmap loaded and validated against target GPIO capabilities
8. RTC-assisted time synchronization attempted (if RTC is configured)
9. Runtime state loaded from NVS
10. Command engine initialized with all built-in commands registered
11. Interactive shell activated
12. All compiled-in modules initialized and started

---

### Logging

Five log levels: `error`, `warn`, `info`, `debug`, `lld` (low-level debug for hardware and bus traces).

- All log lines include a timestamp prefix (monotonic before synchronization, wall clock after)
- Structured tag macros: `HX_LOGE`, `HX_LOGW`, `HX_LOGI`, `HX_LOGD`, `HX_LOGLL`
- In-memory circular ring buffer history (default 8 KB, configurable)
- Dropped line counters for buffer overflow and ISR suppression
- Pluggable sink: output transport is injectable via write callbacks
- Shell-aware redraw hooks: log lines printed while the user is typing erase the prompt, print the log line, then redraw the prompt and the partially typed command
- ISR-safe: log calls from interrupt context are silently dropped and counted

---

### Interactive shell

- Single-line editor with backspace support
- Prompt: `hx>`
- Command dispatch via shared frontend-agnostic command engine
- CRLF and LF line ending normalization
- Transport-agnostic: works over IDF USB Serial JTAG or Arduino USB CDC (selectable at build time)

---

### Command engine — 50+ commands across 8 domains

#### System
| Command | Description |
|---------|-------------|
| `help` / `?` | List all registered commands |
| `reboot` | Soft restart |

#### Logging
| Command | Description |
|---------|-------------|
| `log` | Dump full log history |
| `log clear` | Clear log history buffer |
| `log stat` | Show log level, buffer usage and dropped counts |
| `log level <error\|warn\|info\|debug\|lld>` | Get or set active log level (persisted to config) |

#### Time
| Command | Description |
|---------|-------------|
| `time` | Show full time status: ready, synchronized, source, uptime, UTC |
| `time setepoch <unix_seconds>` | Set time from unix timestamp |
| `time clear` | Clear synchronization state |

#### Configuration
| Command | Description |
|---------|-------------|
| `config list` | Show all visible config keys with current and default values |
| `config read <key>` | Read a single config key |
| `config set <key> <value>` | Set a config key and apply immediately |
| `config toggle <key>` | Toggle a boolean config key |
| `config save` | Persist current config to NVS |
| `config load` | Reload config from NVS |
| `config default` | Reset all values to build defaults |
| `config info` | Show NVS partition info, entry counts and storage usage |
| `config factoryformat` | Format config NVS partition and reset to defaults |

#### State
| Command | Description |
|---------|-------------|
| `state info` | Show NVS usage, key counts, capacity |
| `state format` | Format state NVS partition |
| `state list [prefix]` | List all visible states with type, owner and value |
| `state read <key>` | Read a single state value |
| `state exist <key>` | Check if a state key exists |
| `state create <key> <type> [args]` | Create a runtime state (bool/int/float/string) |
| `state write <key> <value>` | Write a state value |
| `state erase <key>` | Erase persisted value (reset to default) |
| `state delete <key>` | Delete a runtime state |
| `state increment <key>` | Increment an integer state |
| `state decrement <key>` | Decrement an integer state |
| `state toggle <key>` | Toggle a boolean state |

#### Pinmap
| Command | Description |
|---------|-------------|
| `pinmap info` | Show target, GPIO count, mapped pins, binding counts |
| `pinmap list` | Show all GPIO-to-function mappings |
| `pinmap bindings` | List all I2C and UART driver bindings |

#### Runtime
| Command | Description |
|---------|-------------|
| `runtime` | Show full runtime status: RTOS, time, config, pinmap, state, storage, uptime |
| `module list` | List all compiled-in modules with init and start status |
| `module info <name>` | Show status for a specific module |

#### Files (requires `HX_ENABLE_MODULE_STORAGE`)
| Command | Description |
|---------|-------------|
| `files status` | Show active backend, mount state and storage info |
| `files use <flash\|sd>` | Switch active backend |
| `files ls [path]` | List directory contents |
| `files cat <path>` | Read and display a text file |
| `files info <path>` | Show file or directory metadata |
| `files rm <path>` | Remove a file |
| `files mkdir <path>` | Create a directory |
| `files rmdir <path>` | Remove an empty directory |
| `files write <path> <text>` | Write a text file |
| `files append <path> <text>` | Append text to a file |
| `files format` | Format the active backend |

---

### Configuration system

- Schema-driven model with macro-defined keys
- Types: string, int32, bool, float
- Per-key: default value, min/max constraints, console visibility flag, console writability flag
- Persisted to NVS partition `nvs`, namespace `hx_config`
- Applied at boot, re-applicable at runtime via `ConfigApply()`
- Full storage introspection: entry counts, byte usage, visible and writable key counts

---

### State system

- Runtime key registry with both build-time static keys and dynamically created runtime keys
- Types: bool, int32, float, string
- Per-key: owner class (SYSTEM, KERNEL, USER, INTERNAL, EXTERNAL), persistence flag, visibility flags, write restriction flag
- Persisted to NVS with configurable commit delay (default 2 s) to reduce flash wear
- Deferred commit with batching: multiple writes in a short window are coalesced into one NVS commit
- Write sources: USER and SYSTEM tracked separately
- Atomic increment/decrement/toggle operations
- Full storage introspection: static key count, runtime key count, NVS usage, capacity

---

### Pinmap system

- Board pinmap loaded from build-generated JSON embedded in firmware
- Runtime validated against per-chip GPIO capability database (valid, input, output, analog, touch, USB, strap, flash-used, etc.)
- Runtime lookup: function → GPIO, GPIO → function
- I2C and UART driver bindings tracked: type, instance, port, address, control pins
- Bindings exportable as JSON for external tools and web interfaces
- NVS pin override support: individual GPIO assignments can be overridden at runtime without reflashing

---

### I2C subsystem

- Up to 3 I2C buses (hardware-dependent)
- Pins resolved from pinmap at init — no hardcoded GPIO numbers
- On-init address scan: probes 0x08–0x77 on every ready bus and logs found devices
- Per-bus glitch filter (7 APB cycles, configurable)
- Device registry: up to 64 registered devices across all buses
- Per-device availability tracking: 5 consecutive failures marks device unavailable, suppressing error log spam
- Per-bus statistics: successful and failed transactions
- Per-device statistics: tx/rx counts, consecutive failures, last error timestamp
- Bus recovery: 9-clock-pulse SDA sequence to unlock stuck I2C devices
- Transaction types: write, read, write-read (repeated start)
- Thread-safe: mutex-protected per transaction

---

### SPI subsystem

- Up to 2 user SPI buses (SPI2, SPI3 where available)
- Pins resolved from pinmap — MISO optional for TX-only buses
- DMA-backed transfers (configurable max 8 KB)
- Device registry: up to 32 registered devices across all buses
- Per-device availability tracking: 5 consecutive failures threshold
- Per-bus and per-device statistics: tx/rx counts, bytes transferred
- Transaction types: full-duplex, transmit-only
- Thread-safe per transaction

---

### UART subsystem

- Port discovery at startup: scans all mapped ports and logs which are available
- UART ports are not pre-initialized — each port is claimed and configured by its owning driver at runtime with its own baud rate and protocol settings
- RS485 half-duplex support with automatic DE pin control
- Per-port: read, write, flush, available-byte-count, stats
- Per-port statistics: bytes sent, bytes received, write errors, read timeouts
- RX buffer: 1 KB, TX buffer: 256 B (configurable)

---

### Storage subsystem

**LittleFS (internal flash):**
- Thread-safe file operations with mutex protection
- Operations: mount, unmount, format, exists, remove, rename, mkdir, rmdir, stat, list, read, write, append
- Atomic write: write to `.tmp` path, rename to final path — power-safe configuration and asset updates
- Binary and text interfaces
- Storage info: total, used, free bytes

**SD card (SDMMC):**
- Auto bus-width detection (1-bit or 4-bit)
- Board-defined slot, power pin and LDO channel via pinmap
- Card presence detection via CMD13 (hardware command, not VFS polling)
- Mounted at `/sd` via FatFS VFS
- Same operation surface as LittleFS — backend switch is transparent to command layer

---

### Time system

- Monotonic clock always available from hardware (never resets, no wrap risk for practical uptime)
- Synchronized wall clock optional — set from RTC driver at boot via time sync service, or from NTP at runtime
- Time source tracking: NONE, USER, RTC, NTP
- Log timestamps: monotonic before sync, UTC after sync
- Full time info query: ready, synchronized, source, monotonic_ms, unix_ms, sync_age_ms

---

### RTOS abstraction

Minimal FreeRTOS wrapper providing:

- **Critical sections** (`HxRtosCritical`): ISR-safe spinlock for short state protection
- **Mutexes** (`HxRtosMutex`): binary mutex for longer domain protection with timeout support
- Task sleep, yield, ISR context detection, tick counter

All HexaOS core subsystems use these primitives. No subsystem calls FreeRTOS or IDF synchronization APIs directly.

---

### Module lifecycle system

Each optional domain participates in the cooperative runtime via a uniform descriptor:

| Hook | Cadence | Purpose |
|------|---------|---------|
| `init()` | once at boot | Domain initialization |
| `start()` | once after all inits | Post-init activation |
| `loop()` | every loop pass | Continuous polling |
| `every_10ms()` | ~10 ms | Fast soft-periodic work |
| `every_100ms()` | ~100 ms | Normal periodic work |
| `every_1s()` | ~1 s | Slow maintenance work |

Active modules in this release: Storage, I2C, SPI, UART.
Placeholder stubs (disabled): Berry, Web, LVGL.

Module status (ready/started) is tracked and queryable via `module list` and `module info` commands.

---

### Domain scheduler primitive

Reusable per-domain scheduler — not a global scheduler:

- Interval-based with configurable phase offset for stagger across polling domains
- Optional advisory execution budget per firing
- Embedded in caller struct (no heap allocation)
- Simple API: `HxSchedulerDue()`, `HxSchedulerFireNow()`, `HxSchedulerReset()`, enable/disable gate

Used by subsystems that need to plan periodic work independently of the global cadence hooks.

---

### Panic system

Six named panic codes: UNKNOWN, BOOT, NVS, ASSERT, MEMORY, HARDWARE.

- `HX_PANIC(code, reason)` — triggers with auto-captured file and line
- `HX_PANIC_IF(cond, code, reason)` — conditional variant
- Configurable behavior at build time:
  - **Halt mode** (default, debug-friendly): infinite loop with periodic banner repeat
  - **Restart mode** (production): halt for configurable delay then `esp_restart()`

---

### Build-time feature model

All configurable in `hx_build.h`:

**Module selectors:**
- `HX_ENABLE_MODULE_STORAGE` — files/SD subsystem
- `HX_ENABLE_MODULE_I2C` — I2C handler
- `HX_ENABLE_MODULE_SPI` — SPI handler
- `HX_ENABLE_MODULE_UART` — UART handler
- `HX_ENABLE_MODULE_BERRY` / `WEB` / `LVGL` — disabled stubs

**Feature selectors:**
- `HX_ENABLE_FEATURE_LITTLEFS` — internal flash filesystem
- `HX_ENABLE_FEATURE_SD` — SD card support

**Console adapter:**
- `HX_BUILD_CONSOLE_ADAPTER` — IDF USB Serial JTAG or Arduino USB CDC

A disabled feature is architecturally absent — not simply idle. The entire feature stack (module, handler, adapter, commands) is excluded from the build.

---

## What is not yet implemented

The following are planned for future releases and have placeholder stubs or no implementation in this release:

- Web server and web terminal
- Berry scripting engine
- LVGL graphics framework
- Wi-Fi and Ethernet runtime stack
- MQTT / Home Assistant integration
- OTA update pipeline
- NTP time synchronization (service API ready, network stack not yet present)
- RTC driver implementations (DS3232 and HDC2010 referenced in config, driver layer directory exists)
- Central sensor data store (runtime database for driver-published readings)

---

## Architecture guarantees established in this release

- Permanent layer model in code: core, adapters, handlers, drivers, services, modules, commands
- Build-disabled features are fully excluded — no dead code, no dead commands, no stray link dependencies
- All optional modules gated by `#if HX_ENABLE_MODULE_*` from extern declaration through to command registration
- No module depends on another module's init order
- Early-boot work (RTC sync) uses explicit service hook called from `BootInit()` — not a module dependency
- All synchronization primitives go through `system/core/rtos.h` — no raw FreeRTOS or IDF sync calls in handlers or core
- All log output goes through `HX_LOG*` macros — no direct `LogInfo`/`LogWarn`/`LogError` calls in production code

---

## Source tree shape

```
src/
  headers/           shared build and system headers
  hexaos.cpp         thin Arduino entry point
  system/
    core/            boot, log, time, rtos, config, state, pinmap, panic, runtime, module registry, scheduler
    adapters/        I2C, SPI, UART, LittleFS, SDMMC, NVS, console
    handlers/        I2C, SPI, UART, files, user interface
    drivers/         (directory exists, first drivers in next release)
    services/        time_sync_service
    commands/        command engine, cmd_help, cmd_log, cmd_time, cmd_config, cmd_state, cmd_pinmap, cmd_runtime, cmd_files
    modules/         mod_storage, mod_i2c, mod_spi, mod_uart, mod_berry*, mod_web*, mod_lvgl*
    interlibs/       jsonparser
```

`*` — placeholder stub only
