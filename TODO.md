## Sketch of goals for V0.1.0 

## Network Features backed up by mod_network

- Alpha omega - WIFI and settings. Also from start integration of ESP Hosted 
- Define the main WebClient provider (async might use self-developed AsyncHttpClientLight) - crucial ASYNC
- ETHERNET Driver
- Workshop around Wifi Provisioning, fallbacks etc from begin

## WebServer INITIAL Feature

- Define webserver client (self-developed or external library), ASYNC variant only - crucial
- define mod_webserver and handlers or move management under mod_network as service ()
- Define "Hello World" layout for Web Management
- Define the Webserver structure for system management (settings, pinouts, drivers, webconsole etc)

## Async Central Sensoric real-time Database placed in IRAM / PSRAM

- The key - heart engine / service for storing runtime variables pooled from various sensors around system
- Inputs from various services / drivers in infinite pooling state (i2c, UART, internal etc)
- Output to various processing engines / services / protocols (MQTT, WebAPI, HASS, ..)
- Database is NOT a HANDLER. Its owns domain and I/O rules and timming (need to be RTOS driven)
- Input - direct writes, outputs - pointers prohibited, copy allowed, need to handle ISR
- in later stage of development might replace all runtime statistics handling 

## Central event handler / dispatcher for asynchronously managed operations

- crucial bridge between sync and async states / events
- actually in idea stage. Might not replace primitive offset handlers
- event / callback dispatcher



- full webserver implementation,
- OTA implementation,
- command priorities / async command execution buffer,
- low-level memory debugger,
- Wi-Fi and Ethernet full runtime stack,