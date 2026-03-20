## Until VER 0.0.2 release (list not completed yet)

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
- [ ] STATES - Refactor ownership system - no strings, only enums and different logic.
- [ ] STATES - Stable and tested release.
- [ ] CONFIG - Rebuild only AS system (at build) hosted mechanism for configs AND some necessary system counters (reboot counter, last reboot etc). Configs cannot be created / deleted by runtime - only changed by specific rules.
- [ ] FACTORY NVS - Because of lack of use, this NVS will be deleted and not supported in later models. 
- [ ] CONSOLE - Now we have defined only SERIAL console trought HWCDC / JTAG build selector. Need to add typical SERIAL UART as a build selector option (possible serial fallback as optional setting from config with default false.)
- [ ] LOG - Add 5th log level - "LLD", displayng Low Level Debug Messages
- [ ] LOG - Different log levels for different outputs (serial console, web console, terminal etc) - configs + handling
- [ ] LOG - Feature - color / font difference for various log levels / command inputs / outputs
- [ ] COMMANDS - Refactor and prototype of central command register and execution buffer (callbacks, scheduling, priorites, etc)
- [ ] REFACTOR - Final pre-release code refactor.

## Future versions todo by priority

- TIME - We need to create central time engine / interface prepared to operate from RTC and timming for events, synchronysing from internal RTC modules (i2c driver) and NTP sync (prepare, web interface later)
- LOG - Replace timming from TICK to real time (00:00:00 from boot, time after sync from external RTC)
- JSON parsing implementation
- Solve pinout definition mechanism
- filesystem console data handler
- Wifi / Ethernet implementation
- Webserver (start of development)
- OTA handling
- DEBUG - We need console based LOW-LEVEL debugger, capable to call introspect <read/write> <iram/psram/flash/pointer> at selected <hex address> and return/write selected bytes <1,2,4,8,16,32,64...>. Return bytes in HEX. It shall be RAW debugger without any protection (in debug mode, crashes are acceptable.)