# Milestone 5 Notes

This repository now includes a first Milestone 5 bring-up pass focused on I2C visibility and safe peripheral probing.

## Current Milestone 5 Scope

Implemented in the current codebase:

- I2C initialization on the board-default bus
- bus scan across `0x08` to `0x77`
- explicit probe output for RTC, LCD, and expected Wiegand processor addresses
- DS3231-style RTC register read at `0x68`
- non-destructive Wiegand `JMP`/`VER` probe reporting for detected processor addresses

Not yet implemented in this milestone slice:

- actual I2C command exchange with Wiegand processors
- LCD initialization/write logic
- generalized RTC abstraction

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
- Wiegand processor addresses `0x30` to `0x37` are checked, and the firmware prints which `VER` and `JMP` command would be used if those devices are present.

## What To Test Right Now

With the current firmware loaded, serial output should show:

1. Milestone 5 boot banner
2. `I2C: initializing i2c0 on SDA=GP4 SCL=GP5`
3. every detected address on the bus
4. explicit status for RTC, LCD, and Wiegand processor slots
5. RTC time output if the device at `0x68` behaves like a DS3231

## Next Planned Seam

After this bus scan is confirmed, the next step should be:

1. add real `VER` and `JMP` I2C command transactions for processor addresses that exist
2. add LCD initialization if a compatible backpack is present
3. separate RTC handling into its own manager
