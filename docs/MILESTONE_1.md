# Milestone 1 Notes

This repository currently implements only the Milestone 1 scope from [`CODEX_SPEC.md`](CODEX_SPEC.md):

- Pico SDK C++ project skeleton
- serial debug output
- status LED heartbeat
- no Ethernet
- no MQTT
- no SD card support

## Architecture Notes

- `src/main.cpp` is intentionally thin and only performs board startup plus app handoff.
- `src/core/App.*` owns application lifecycle and the main non-blocking service loop.
- `src/devices/LedManager.*` encapsulates LED timing and GPIO state control.
- `src/config/BootConfig.h` holds temporary boot-time defaults needed before `config.json` exists in Milestone 2.

## Temporary Defaults

The spec requires config-owned values to come from `config.json`. Since SD/config loading is not part of Milestone 1, the scaffold uses a minimal temporary boot configuration only for:

- LED heartbeat timing
- LED GPIO selection

Where possible, the LED pin is taken from `PICO_DEFAULT_LED_PIN` supplied by the board definition. If the board definition does not provide one, the firmware will report an LED initialization error instead of silently hard-coding a guessed pin.

## Next Planned Seam

Milestone 2 should replace the static boot config source with:

1. SD card mount support
2. `config.json` loading
3. validation of required sections
4. translation from parsed config into runtime manager settings
