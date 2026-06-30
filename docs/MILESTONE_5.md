# Milestone 5 Notes

This repository now includes an active Milestone 5 bring-up pass focused on live I2C protocol validation against downstream RP2350 Wiegand devices.

## Current Milestone 5 Scope

Implemented in the current codebase:

- I2C initialization on the board-default bus
- bus scan across `0x08` to `0x77`
- explicit probe output for RTC, LCD, and expected Wiegand processor addresses
- DS3231-style RTC register read at `0x68`
- extracted `I2cCommandTransport`, `PiWiegandI2cDevice`, and `PiWiegandDeviceManager` classes
- live `0x78` build-info reads for present downstream devices
- live `0x79` event reads for present downstream devices
- live `0x21` Wiegand-out status reads for present downstream devices
- live `0x20` Wiegand-out send on interface `0`

Not yet implemented in this milestone slice:

- LCD initialization/write logic
- generalized RTC abstraction
- generic Wiegand bitstring builder from `config/config.json` and `config/wiegand.json`
- MQTT-driven downstream Wiegand send path

## Hardware Mapping Used

The current firmware uses the board default I2C mapping:

- `i2c0`
- `SDA = GP4`
- `SCL = GP5`

On your current wiring, that matches:

- physical pin `6` -> `GP4` -> `SDA`
- physical pin `7` -> `GP5` -> `SCL`

## Important Current Behavior

- The firmware prints every responding I2C address during boot.
- RTC `0x68` is read using a DS3231-compatible register layout.
- LCD addresses `0x27` and `0x3F` are checked but not initialized yet.
- Wiegand processor addresses `0x30` to `0x37` are checked through shared per-device classes.
- Present devices are queried with `0x78` build-info.
- Present devices are queried with `0x21` Wiegand-out status.
- Present devices are queried with `0x79` event-read.
- Interface `0` currently performs one fixed `0x20` Wiegand send test using bitstring `10101010`.

## Verified Hardware Results

The following live behavior has already been verified:

1. `0x30` returns build-info text such as `version=0.1.0 git=e6fc36f built_utc=...`
2. `0x31` returns build-info text and may be on a different build timestamp
3. `0x21` returns `57 53 21 00 00` when idle
4. `0x20` returns `57 4F 00 20` on successful queue
5. after `0x20`, `0x21` can report `busy=1 queued=0`
6. an external Wiegand sniffer on `0x30` confirmed transmit `8,10101010`
7. `0x79` returns both queue-empty headers and live system events depending on device state

## Next Planned Seam

After these live protocol checks, the next step should be:

1. parse per-address output format and facility-code selection from `config/config.json`
2. parse format definitions and parity masks from `config/wiegand.json`
3. implement a generic C++ Wiegand builder based on the proven Python helper logic
4. replace the fixed `10101010` test frame with a real generated `wiegand_48` frame
5. keep MQTT disconnected from this send path until the generic builder is proven on hardware
