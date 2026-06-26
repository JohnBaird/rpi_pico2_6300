# LittleFS Config Store

## Purpose

The controller now keeps its active runtime configuration in onboard flash using LittleFS.

Startup order is:

1. Mount LittleFS
2. Load `/config.json` from LittleFS
3. Verify `crc32`
4. Validate required config sections
5. If invalid or missing, restore the factory default config

The factory default source file in the repo is [config/config.json](/e:/Development/rpi_pico2_6300/config/config.json).

## Flash Allocation

- Total board flash: `2 MB`
- Reserved LittleFS partition: `256 KB`
- LittleFS offset: top `256 KB` of flash

This partition is reserved for persistent config and light diagnostics. It reduces the maximum available firmware image size by `256 KB`.

## Files

- `/config.json`
  Active runtime config
- `/config.bak.json`
  Seeded backup copy
- `/config.meta`
  Active source, CRC32, and last event
- `/error.log`
  Small rolling error/event log

## Factory Default

The checked-in factory config contains:

```json
"crc32": "00000000"
```

At boot, firmware computes the correct CRC32 and writes it into the in-memory factory copy before seeding LittleFS or using the factory fallback.

## Recovery Behavior

- If LittleFS mount fails, firmware formats the reserved partition and mounts it again.
- If `/config.json` is missing, firmware seeds LittleFS from the factory config.
- If `/config.json` fails CRC or schema checks, firmware attempts `/config.bak.json`.
- If both are invalid, firmware restores the factory config.

## Planned Next Steps

- import `/config.json` from SD card into LittleFS
- MQTT command to replace LittleFS `/config.json`
- explicit factory reset / filesystem rebuild command
