# W6300 Access Gateway Firmware

Pico SDK C++ firmware scaffold for the WIZnet W6300-EVB-Pico2 access gateway.

## Documentation

- Primary specification: [`docs/CODEX_SPEC.md`](docs/CODEX_SPEC.md)
- Milestone 1 notes: [`docs/MILESTONE_1.md`](docs/MILESTONE_1.md)
- Milestone 2 notes: [`docs/MILESTONE_2.md`](docs/MILESTONE_2.md)
- Milestone 3 notes: [`docs/MILESTONE_3.md`](docs/MILESTONE_3.md)
- Milestone 4 notes: [`docs/MILESTONE_4.md`](docs/MILESTONE_4.md)
- Milestone 5 notes: [`docs/MILESTONE_5.md`](docs/MILESTONE_5.md)
- LittleFS config store: [`docs/LITTLEFS_CONFIG.md`](docs/LITTLEFS_CONFIG.md)
- Environment setup: [`docs/ENVIRONMENT_SETUP.md`](docs/ENVIRONMENT_SETUP.md)

## Repository Layout

```text
.
|-- CMakeLists.txt
|-- pico_sdk_import.cmake
|-- docs/
|   |-- CODEX_SPEC.md
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
- modular `App`, `LedManager`, `SdCardManager`, `ConfigManager`, `I2cManager`, `EthernetManager`, and `MqttManager` classes

Full LCD behavior, richer RTC support, and the wider access-control application logic are still deferred to later milestones.
