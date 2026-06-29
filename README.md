# W6300 Access Gateway Firmware

Pico SDK C++ firmware scaffold for the WIZnet W6300-EVB-Pico2 access gateway.
https://eshop.wiznet.io/shop/module/w6300-evb-pico2/

## Documentation

- Primary specification: [`docs/CODEX_SPEC.md`](docs/CODEX_SPEC.md)
- Milestone 1 notes: [`docs/MILESTONE_1.md`](docs/MILESTONE_1.md)
- Milestone 2 notes: [`docs/MILESTONE_2.md`](docs/MILESTONE_2.md)
- Milestone 3 notes: [`docs/MILESTONE_3.md`](docs/MILESTONE_3.md)
- Milestone 4 notes: [`docs/MILESTONE_4.md`](docs/MILESTONE_4.md)
- Milestone 5 notes: [`docs/MILESTONE_5.md`](docs/MILESTONE_5.md)
- Access-control design: [`docs/ACCESS_CONTROL_DESIGN.md`](docs/ACCESS_CONTROL_DESIGN.md)
- Next milestones: [`docs/NEXT_MILESTONES.md`](docs/NEXT_MILESTONES.md)
- LittleFS config store: [`docs/LITTLEFS_CONFIG.md`](docs/LITTLEFS_CONFIG.md)
- Environment setup: [`docs/ENVIRONMENT_SETUP.md`](docs/ENVIRONMENT_SETUP.md)

## Working Rules

`README.md` is the high-level project index and status page. Detailed design guidance belongs under `docs/`. When requirements change, the relevant design document and this README index should both be updated so the project does not drift.

## Git History

Use `git rev-parse --short HEAD` to see the current short git number from the repo.

- `ae9bafe`
  - Commit message: `Add DS2401 startup MAC validation and access-control design notes`
  - Recorded: `2026-06-27 20:30 America/New_York`
- `da10b4e`
  - Commit message: `Add littlefs dependency`
  - Recorded: `2026-06-26 18:20 America/New_York`
- `1b30a85`
  - Commit message: `Initial firmware scaffold`
  - Recorded: `2026-06-26 18:18 America/New_York`

## Repository Layout

```text
.
|-- CMakeLists.txt
|-- pico_sdk_import.cmake
|-- docs/
|   |-- CODEX_SPEC.md
|   |-- ACCESS_CONTROL_DESIGN.md
|   |-- NEXT_MILESTONES.md
|   |-- MILESTONE_1.md
|   |-- MILESTONE_2.md
|   |-- MILESTONE_3.md
|   |-- MILESTONE_4.md
|   |-- MILESTONE_5.md
|   `-- LITTLEFS_CONFIG.md
`-- src/
    |-- main.cpp
    |-- config/
    |-- core/
    |-- devices/
    |-- i2c/
    |-- mqtt/
    |-- network/
    `-- storage/
```

## Milestone Status

The repo now has working Milestone 1 and Milestone 2 bring-up, plus Milestone 3 Ethernet, Milestone 4 MQTT, and a first Milestone 5 I2C bring-up implementation ready for hardware verification:

- Pico SDK project skeleton
- official C++ / Pico SDK workflow
- serial debug startup output
- non-blocking LED heartbeat
- SD-backed `/config.json` loading
- DHCP/static Ethernet startup using the W6300 stack
- MQTT connect, subscribe, and status publish
- I2C scan and RTC/LCD/Wiegand address probing
- DS2401 serial read at startup with MAC override priority over config
- startup serial gate on `GP22` so reset messages can be captured on demand
- Ethernet IP address printed during startup
- modular `App`, `LedManager`, `SdCardManager`, `ConfigManager`, `I2cManager`, `EthernetManager`, and `MqttManager` classes

Full LCD behavior, richer RTC support, and the wider access-control application logic are still deferred to later milestones.

## Access-Control Design

Detailed MQTT routing rules, I2C address mapping, class boundaries, PACS controller scope, and lessons from the single-device reference project now live in [`docs/ACCESS_CONTROL_DESIGN.md`](docs/ACCESS_CONTROL_DESIGN.md).

## Next Milestones

The recommended next milestone path now lives in [`docs/NEXT_MILESTONES.md`](docs/NEXT_MILESTONES.md).
