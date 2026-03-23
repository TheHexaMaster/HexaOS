## V0.1.0 — Chronological plan

### 1. Network foundation
- [ ] Define `mod_network` architecture — abstraction over transport (WiFi / ESP-Hosted / ETH same API)
- [ ] WiFi driver — init, connect, disconnect, event callbacks (connected, lost, IP acquired)
- [ ] ESP-Hosted integration as transport backend behind `mod_network` (P4 path, mandatory)
- [ ] Config persistence for network settings (SSID, password, mode) via existing config handler

### 2. WiFi provisioning
- [ ] AP fallback mode (if credentials missing or connection fails N times)
- [ ] Provisioning flow (setup via AP + captive portal or serial console command)
- [ ] Fallback policy: retry order, timeout, reset to AP

### 3. Ethernet driver
- [ ] Ethernet driver under `mod_network` (same interface as WiFi)
- [ ] Link-up / link-down event handling
- [ ] WiFi + ETH coexistence (prioritization, failover)

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



Kde je abstrakcia
Cieľ nie je abstrahovať fyzický transport (WiFi vs. ETH sú fundamentálne rôzne vrstvy), ale abstrahovať IP konektivitu — výsledok je vždy rovnaký: zariadenie má IP adresu a môže komunikovať po sieti.


mod_network  (HexaOS module — lifecycle)
    └── network_handler  (domain owner — stav, logika, API)
         ├── wifi_adapter     (WiFi + ESP-Hosted za jednou fasádou)
         └── eth_adapter      (Ethernet)
mod_network vlastní stav siete. Nič nad ním sa nestará o to, či je aktívny WiFi, ESP-Hosted alebo ETH.

Build gating

HX_ENABLE_MODULE_NETWORK
HX_ENABLE_FEATURE_WIFI          — natívna WiFi (ESP32 built-in)
HX_ENABLE_FEATURE_ESP_HOSTED    — sub-flag: ESP-Hosted ako WiFi transport
HX_ENABLE_FEATURE_ETHERNET      — RMII alebo SPI Ethernet
ESP-Hosted nie je samostatný modul — je to alternatívny init path v wifi_adapter. Na IDF vrstve sa po inicializácii tvári ako štandardná esp_netif WiFi interface.

Kľúčové rozhodnutia pred implementáciou
1. Single-active vs. dual-active transport

Máš ETH + WiFi/ESP-Hosted súčasne? Dve možnosti:

Primary/secondary s failover: ETH je primárny, WiFi fallback. Jednoduchší state machine.
Dual-active: obe rozhrania aktívne, routing rozhoduje IP stack. Komplexnejšie, ale reálne pre P4 priemyselné nasadenie.
Návrh: začni s primary/secondary, dual-active do LATER.

2. Event model — ako informovať ostatné moduly

Štyri prístupy:

Prístup	Výhoda	Nevýhoda
Runtime flag Hx.net_connected	jednoduchý, polling	žiadna reakcia na zmenu
Callback registrácia	priama notifikácia	spravovať zoznam callbackov
Polling NetworkIsConnected()	bezstavový	meškaná reakcia
Event queue (budúci event handler)	čistá async architektúra	závislosť na #5
Návrh: runtime flag + callback registrácia pre V0.1.0. Event queue príde prirodzene keď bude #5 hotový — vtedy callbacky nahradíš.

3. Reconnection logika — kde žije

V adaptéri: každý adapter rieši vlastné retries → duplicitná logika
V handleri: centrálny state machine, adapter len hlási "up/down" → čistejšie
Návrh: retry logika v network_handler, adapter je hlúpy — init, connect, disconnect, status.

4. WiFi provisioning state machine


IDLE → CONNECTING → CONNECTED
              ↓ (N fails)
           AP_MODE → PROVISIONING → CREDENTIALS_SAVED → CONNECTING
Kde žije? V network_handler — nie v adaptéri, pretože provisioning je business logika, nie transport detail.

5. IP management

DHCP default (takmer vždy)
Static IP z config handlera ako override
DHCP hostname (hx-{chip_id} alebo konfigurovateľný)
Navrhovaná public API network_handler.h

// Lifecycle (volá mod_network)
bool NetworkInit();
bool NetworkStart();

// Status
bool        NetworkIsConnected();
const char* NetworkGetIP();
const char* NetworkGetTransportName();  // "wifi", "esp_hosted", "eth"

// Transport selection (z build flags, nie runtime switching)
// — aktívny transport je compile-time decision pre V0.1.0

// Callbacks
typedef void (*HxNetworkEventCb)(bool connected, void* user);
bool NetworkRegisterEventCallback(HxNetworkEventCb cb, void* user);

// Commands (volá cmd_network.cpp)
bool NetworkConnect();
bool NetworkDisconnect();
bool NetworkGetInfo(HxNetworkInfo* out);
Čo potrebujem od teba pred tým než navrhneme štruktúru súborov a pôjdeme do kódu:

Primary transport pre prvý board — na čom budeš testovať V0.1.0? ESP32 s natívnou WiFi, alebo rovno P4 s ESP-Hosted?
Dual-active súhlas? — potvrdíš primary/secondary pre V0.1.0?
Callback limit — koľko maximálne konsumentov bude počúvať na network events? (MQTT, WebServer, možno 2-3 ďalšie) — stačí statické pole 8 slotov?