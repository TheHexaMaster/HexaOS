## Until VER 0.0.2 release (list not completed yet)

- [x] Header system and build / config schema refactor
- [x] Separate console and log system (done 19.03.2026)
- [x] Command engine separated from console to custom layer (preparation for future extensions - web console, telnet etc)
- [x] STATES - We need to finish engine to be ready to work with internal / externally defined states (modules, drivers, user-defined scripted states from scripting engine / console engine in future etc)
- [x] STATES - Console command handling for states, updated handler for increment / decrement for integers, toggle for booleans
- [ ] STATES - Console / module persistence question to solve
- [ ] STATES - Cashing handler / save delay, settable in config settings (to protect NVS from too-fast write cycles, suggested minimum - 1s).
- [ ] Define serial / fallback to serial console adapter in addition to CDC / JTAG
- [ ] TIME - We need to create central time engine / interface prepared to operate from RTC and timming for events, synchronysing from internal RTC modules (i2c driver) and NTP sync (prepare, web interface later)
- [ ] LOG - Replace timming from TICK to real time (00:00:00 from boot, time after sync from external RTC)