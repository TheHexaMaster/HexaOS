## HexaOS - ESP32 Universal operating system and development platform - Flash Partition Layout

The following partition model is intended to be shared across both 4 MiB and 16 MiB flash variants.
Support for 1 MiB flash devices is not considered anymore.

1. Bootloader / reserved area  
Standard ESP reserved region from 0x00000 up to 0x09000, including the bootloader and partition table area.

2. OTA data partition
Standard OTA metadata partition located immediately after the reserved region.
Size: 0x2000 (8 KiB).

3. Factory device data NVS
Optional read-only or manufacturing-oriented NVS partition reserved for per-device factory data in case device-specific manufacturing data is needed in the future.
Suggested fixed size: 16 KiB.

4. Configuration NVS
This partition stores runtime configuration overrides.
Build-time default settings remain part of the firmware image; only settings modified at runtime are persisted here.
Suggested size: 128 KiB.
This should be sufficient for a large number of configuration keys, while still leaving headroom for internal NVS wear leveling and page rotation.

5. State NVS
This partition stores persistent runtime state that should survive reboot, such as relay states, switch states, selected runtime flags, or user-defined script state.
It should not be used as a general-purpose high-frequency telemetry log.
For very fast-changing values, users should be encouraged to store data in FRAM (ferroelectric random-access memory) or external EEPROM instead of internal flash.
Suggested size: 64 KiB.

6. Safeboot partition
Minimal recovery image used for OTA recovery, debug access, and factory reset handling if the main application fails to boot repeatedly.
For 4 MiB flash devices, the practical size is 832 KiB.
For 16 MiB and larger devices, the suggested size is 2560 KiB, which also allows an optional display-based recovery environment.

7. Main application partition
Main HexaOS firmware partition located after safeboot. Suggested maximum size:
- 2560 KiB for 4 MiB flash devices
- 5120 KiB for 16 MiB and larger devices

8. LittleFS partition
Remaining flash space is assigned to LittleFS for user data such as scripts, assets, and runtime files.

## HexaOS partition layout overview

HexaOS uses the standard ESP32 flash boot flow.

### Bootloader and partition table mapping

The ESP32 second-stage bootloader is normally flashed at offset `0x1000`.  
The partition table is normally flashed at offset `0x8000`, and it occupies one full flash sector (`0x1000`, 4 KiB), including its integrity metadata.  
Because of this, the first custom data partition typically starts at offset `0x9000`.  
This is why the HexaOS layouts below begin with `otadata` at `0x009000`.

If the bootloader ever grows too large and approaches the default partition table location, the partition table offset can be moved higher. In that case, all following partition offsets must be adjusted accordingly.

### OTA boot selection

HexaOS uses an `otadata` partition to store OTA boot metadata.  
The bootloader reads the partition table, detects available application partitions, and then uses `otadata` to decide which image should be booted.

---

## Review of final partitions configuration for HexaOS 4 MiB flash

Index | Name            | Type            | SubType   | Offset   | Size     | KiB  | Description
------|-----------------|-----------------|-----------|----------|----------|------|--------------------------------------------------------
0     | bootloader      | bootloader      | primary   | 0x001000 | 0x007000 | 28   | ESP32 second-stage bootloader area (mapped up to partition table)
1     | partition_table | partition_table | primary   | 0x008000 | 0x001000 | 4    | ESP32 primary partition table
2     | otadata         | data            | ota       | 0x009000 | 0x002000 | 8    | OTA metadata partition
3     | nvs_factory     | data            | nvs       | 0x00B000 | 0x004000 | 16   | Optional per-device factory data
4     | nvs             | data            | nvs       | 0x00F000 | 0x020000 | 128  | Runtime configuration overrides
5     | nvs_state       | data            | nvs       | 0x02F000 | 0x010000 | 64   | Persistent runtime state
6     | safeboot        | app             | factory   | 0x040000 | 0x0D0000 | 832  | Minimal recovery / debug / factory reset image
7     | app0            | app             | ota_0     | 0x110000 | 0x280000 | 2560 | Main HexaOS application image
8     | littlefs        | data            | littlefs  | 0x390000 | 0x070000 | 448  | User filesystem for scripts, assets and runtime files

---

## Review of final partitions configuration for HexaOS 16 MiB flash

Index | Name            | Type            | SubType   | Offset   | Size     | KiB  | Description
------|-----------------|-----------------|-----------|----------|----------|------|--------------------------------------------------------
0     | bootloader      | bootloader      | primary   | 0x001000 | 0x007000 | 28   | ESP32 second-stage bootloader area (mapped up to partition table)
1     | partition_table | partition_table | primary   | 0x008000 | 0x001000 | 4    | ESP32 primary partition table
2     | otadata         | data            | ota       | 0x009000 | 0x002000 | 8    | OTA metadata partition
3     | nvs_factory     | data            | nvs       | 0x00B000 | 0x004000 | 16   | Optional per-device factory data
4     | nvs             | data            | nvs       | 0x00F000 | 0x020000 | 128  | Runtime configuration overrides
5     | nvs_state       | data            | nvs       | 0x02F000 | 0x010000 | 64   | Persistent runtime state
6     | safeboot        | app             | factory   | 0x040000 | 0x280000 | 2560 | Recovery image, optionally with LVGL support
7     | app0            | app             | ota_0     | 0x2C0000 | 0x500000 | 5120 | Main HexaOS application image
8     | littlefs        | data            | littlefs  | 0x7C0000 | 0x840000 | 8448 | User filesystem for scripts, assets and runtime files

---

## Notes
- `bootloader` and `partition_table` rows are included here for documentation clarity. They describe the standard ESP32 flash mapping, but they are not normally written as regular data/app rows in the final custom partition CSV.
- `nvs_factory` is reserved for optional per-device manufacturing data.
- `nvs` stores runtime configuration overrides only. Build-time defaults remain part of the firmware image.
- `nvs_state` is intended for persistent runtime state that should survive reboot. It is not intended for high-frequency logging or telemetry history.
- `safeboot` is a dedicated recovery image used when the main application fails repeatedly or when a recovery / factory reset path is required.
- `littlefs` uses the remaining free flash space for user scripts, assets, and runtime files.
