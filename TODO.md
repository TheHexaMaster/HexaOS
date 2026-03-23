## V0.1.0 — Chronological plan

### 1. Network foundation
- [x] Define `mod_network` architecture — abstraction over transport (WiFi / ESP-Hosted / ETH same API)
- [x] WiFi driver — init, connect, disconnect, event callbacks (connected, lost, IP acquired)
- [x] ESP-Hosted integration as transport backend behind `mod_network` (P4 path, mandatory)
- [x] Config persistence for network settings (SSID, password) via existing config handler

### 2. WiFi provisioning — LATER
- [ ] AP fallback mode (if credentials missing or connection fails N times)
- [ ] Provisioning flow (setup via AP + captive portal or serial console command)
- [ ] Fallback policy: retry order, timeout, reset to AP

### 3. Ethernet driver
- [x] Ethernet driver under `mod_network` (same interface as WiFi)
- [x] Link-up / link-down event handling
- [x] WiFi + ETH coexistence (prioritization, failover)

### 4. Async Central Sensoric Database (IRAM / PSRAM)
- [ ] Define schema: variable type, timestamp, source, TTL
- [ ] IRAM / PSRAM allocation — size and layout decision
- [ ] Write API (task context, lock-free where possible)
- [ ] Read API (copy-only, no pointers out)
- [ ] RTOS task for input scheduling (no busy loop)
- [ ] ISR-safe write bridge (stage 1: basic, ISR queue in stage 2)

### 5. Central event handler / dispatcher
- [ ] Define event types and priority classes
- [ ] Async command execution buffer (RTOS queue)
- [ ] Subscribe/callback registration for consumers
- [ ] Sync → async bridge (caller submits event, does not block)

### 6. MQTT
- [ ] MQTT client under event handler (publish = queue event → async send)
- [ ] Connect/disconnect with retry logic
- [ ] Subscribe handling via event dispatcher
- [ ] Sensoric DB → MQTT bridge (periodic export)
- [ ] Config for broker, port, credentials, topic prefix

### 7. AsyncHttpClient integration
- [ ] Port own AsyncHttpClient from Tasmota into HexaOS structure
- [ ] Adapt to `mod_network` abstraction (not directly WiFiClient)
- [ ] Smoke test — GET/POST request

### 8. WebServer — foundation
- [ ] Confirm library (ESPAsyncWebServer or custom)
- [ ] `mod_webserver` module — init, start, stop under `mod_network`
- [ ] Hello World endpoint — `/` returns "HexaOS is alive" + version

### 9. WebServer — management structure
- [ ] Define URL scheme (`/api/`, `/ui/`, `/console/` etc.)
- [ ] System settings page (network, pinmap read-only view)
- [ ] WebConsole — WebSocket bridge to command engine
- [ ] Sensoric DB live view endpoint (`/api/sensors`)

### LATER
- [ ] OTA — decision: IDF atomic OTA vs. custom (affects partition scheme)
- [ ] Low-level memory debugger
