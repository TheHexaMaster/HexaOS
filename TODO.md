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
- [ ] RTOS - REfactor - move from handler to core without using build selector - native RTOS core implementation. This way core RTOS can manage different RTOS adapters in future. 
- [ ] RTOS - Integration PART 2 - update to existing modules using RTOS external unmanaged calls - NVS config and STATE. 
- [ ] TIME - We need to create central time engine / interface prepared to operate from RTC and timming for events, synchronysing from internal RTC modules (i2c driver) and NTP sync (prepare, web interface later)
- [ ] LOG - Add 5th log level - "LLD", displayng Low Level Debug Messages
- [ ] LOG - Different log levels for different outputs (serial console, web console, terminal etc) - configs + handling
- [ ] LOG - Feature - color / font difference for various log levels / command inputs / outputs
- [ ] LOG - Replace timming from TICK to real time (00:00:00 from boot, time after sync from external RTC)
- [ ] COMMANDS - Refactor and prototype of central command register and execution buffer (callbacks, scheduling, priorites, etc) (maybe RTOS adapter / handler first?)
- [ ] CONSOLE - Now we have defined only SERIAL console trought HWCDC / JTAG build selector. Need to add typical SERIAL UART as a build selector option (possible serial fallback as optional setting from config with default false.)
- [ ] REFACTOR - Final pre-release code refactor.

## Future versions todo by priority

- 
- JSON parsing implementation
- Solve pinout definition mechanism
- filesystem console data handler
- Wifi / Ethernet implementation
- Webserver (start of development)
- OTA handling
- DEBUG - We need console based LOW-LEVEL debugger, capable to call introspect <read/write> <iram/psram/flash/pointer> at selected <hex address> and return/write selected bytes <1,2,4,8,16,32,64...>. Return bytes in HEX. It shall be RAW debugger without any protection (in debug mode, crashes are acceptable.)