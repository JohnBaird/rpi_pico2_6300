# Milestone 2 Notes

This repository now includes a working Milestone 2 path on top of the proven Milestone 1 bring-up.

## Current Milestone 2 Scope

Implemented in the current codebase:

- `ConfigManager` startup flow
- `RuntimeConfig` structure for loaded values
- SPI SD card initialization and mount flow
- `SdCardManager` file-read path for `/config.json`
- required-section validation for a minimal `config.json` shape
- serial printout of loaded configuration values during boot

Not yet implemented in this milestone slice:

- JSON parser suitable for the full production schema
- runtime application of loaded config to Ethernet, MQTT, LCD, RTC, or interface managers

## Architecture Notes

- `src/core/App.*` now initializes LED, SD card access, and config loading in startup order.
- `src/storage/SdCardManager.*` now mounts the SD card and reads `/config.json` through FatFs.
- `src/config/ConfigManager.*` owns Milestone 2 config loading, validation, and summary output.
- `src/config/RuntimeConfig.h` provides the first runtime config model used by the app.
- `src/storage/SdHwConfig.c` binds the SD driver to `SPI1` with `SCK=GP10`, `MOSI=GP11`, `MISO=GP12`, and `CS=GP13`.

## Important Current Behavior

Milestone 2 now has a real SD-backed configuration path.

`SdCardManager` initializes the SD driver, mounts the card, and attempts to read `/config.json` from physical media. If SD initialization, mount, or file read fails, `ConfigManager` still falls back to an embedded temporary `config.json` string so the device can continue to exercise the startup path and print a clear serial summary.

This means:

- the firmware **does** attempt to read `/config.json` from an SD card first
- the printed device MAC is **not** yet a live Ethernet MAC on the network
- the startup flow is valid for SD-backed config loading, but not yet a finished full-schema configuration subsystem

## Temporary Embedded Fallback

The current fallback configuration covers only the minimum fields needed to prove the Milestone 2 flow:

- `device.name`
- `device.mac`
- `ethernet.mode`
- `mqtt.broker`
- `mqtt.port`
- `led.healthy_on_ms`
- `led.healthy_off_ms`

This remains a temporary development aid and is only used when the real SD-backed file read path fails.

## What To Test Right Now

With the current firmware loaded, serial output should show:

1. Milestone 2 boot banner
2. LED initialization success
3. SD driver initialization
4. successful mount and config read from the card, or a clear fallback warning if SD/config read fails
5. printed config summary values
6. normal non-blocking LED heartbeat continuing

## Next Planned Seam

To complete Milestone 2 properly, the next implementation step should be:

1. verify `/config.json` is read from the inserted card on real hardware
2. remove fallback dependence once card access is stable
3. expand validation closer to the full spec schema
4. support more required sections from the specification
5. keep serial summary output for hardware verification
