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
- LittleFS config store: [`docs/LITTLEFS_CONFIG.md`](docs/LITTLEFS_CONFIG.md)
- Environment setup: [`docs/ENVIRONMENT_SETUP.md`](docs/ENVIRONMENT_SETUP.md)

## Working Rules

`README.md` is the running project reference for architecture, message-routing rules, and device-mapping decisions. When requirements change, this file should be updated alongside the implementation so the project does not drift.

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
- DS2401 serial read at startup with MAC override priority over config
- startup serial gate on `GP22` so reset messages can be captured on demand
- Ethernet IP address printed during startup
- modular `App`, `LedManager`, `SdCardManager`, `ConfigManager`, `I2cManager`, `EthernetManager`, and `MqttManager` classes

Full LCD behavior, richer RTC support, and the wider access-control application logic are still deferred to later milestones.

## Access-Control Integration Rules

These rules define how the W6300 gateway will translate MQTT requests into I2C commands for up to 8 downstream RP2350 Wiegand devices.

### MQTT topic families

- Identification system topics use `SPV1.0/ident/...`.
- Monitoring system topics use `SPV1.0/access/...`.
- If the topic contains `ident`, the message is an identification request from the identification system.
- If the topic contains `access`, the message belongs to the monitoring system and follows a related but not identical ruleset.

### Identification-system rule

- `ident` requests are never answered back to the `ident` topic family.
- `ident` requests must be logged or forwarded into the `access` monitoring path.
- The gateway is responsible for validating all Wiegand details before anything is sent to an I2C device.

Example identification message:

- Topic: `SPV1.0/ident/roc_access_request/51918423475411/251096704254951`
- Payload fields of interest:
  - `accessDevice`
  - `accessPort`
  - `badgeId`
  - `personId`
  - `firstname`
  - `lastname`
  - `timestamp`
  - `mqtt_target`

### I2C address mapping

The gateway supports a maximum of 8 downstream devices on I2C addresses `0x30` through `0x37`.

- `accessPort = "0"` maps to `0x30`
- `accessPort = "1"` maps to `0x31`
- `accessPort = "2"` maps to `0x32`
- `accessPort = "3"` maps to `0x33`
- `accessPort = "4"` maps to `0x34`
- `accessPort = "5"` maps to `0x35`
- `accessPort = "6"` maps to `0x36`
- `accessPort = "7"` maps to `0x37`

This mapping should stay fixed so that application logic can consistently treat `accessPort` as the logical interface number.

### Responsibility split

- MQTT parsing and routing happen in this gateway.
- Wiegand format validation happens in this gateway.
- I2C devices do not decide whether incoming Wiegand details are valid.
- Each I2C device may use its own Wiegand format, but the policy and translation logic still live here.
- Downstream I2C devices should receive normalized commands rather than raw upstream business logic.

### Class-design direction

To avoid duplicated logic, the access-control path should be implemented with shared classes instead of per-port special cases.

- One transport layer should own low-level I2C exchange details.
- One protocol layer should own packet encoding, decoding, command IDs, and reply parsing.
- One device class should represent a single downstream interface regardless of whether it is at `0x30` or `0x37`.
- One manager or router should map MQTT `accessPort` values to device instances and apply the same workflow to every port.
- Topic-family differences such as `ident` versus `access` should be handled in a centralized rule layer, not copied into each device path.

### Planned class API

Each downstream RP2350 node should be treated as a full PACS door controller, not as a narrow Wiegand-only endpoint. The API should therefore leave room for reader input, lock control, request-to-exit, door position, supervision, status, alarms, and future controller features.

#### `I2cDeviceTransport`

Purpose: own the physical I2C transaction layer.

- `bool probe(uint8_t address)`
- `bool write_command(uint8_t address, const uint8_t* data, size_t length)`
- `bool read_reply(uint8_t address, uint8_t* data, size_t length)`
- `bool exchange(uint8_t address, const uint8_t* tx, size_t tx_length, uint8_t* rx, size_t rx_length)`
- `void set_retry_policy(...)`
- `void set_timeout_ms(...)`

Responsibilities:

- Bus access
- retry behavior
- timeout handling
- common transport-level error reporting

#### `Rp2350PacsProtocol`

Purpose: own packet definitions, command codes, payload builders, and response parsers for the downstream PACS controller protocol.

Suggested API:

- `build_wiegand_output_command(...)`
- `parse_wiegand_output_status(...)`
- `build_output_set_command(...)`
- `build_output_pulse_command(...)`
- `parse_output_status(...)`
- `build_rgb_set_command(...)`
- `build_rgb_pulse_command(...)`
- `parse_rgb_status(...)`
- `build_runtime_config_upload(...)`
- `parse_runtime_config(...)`
- `parse_build_info(...)`
- `parse_next_event(...)`
- `build_restart_command()`
- `build_bootloader_command()`

Responsibilities:

- command serialization
- response validation
- payload decoding
- conversion between raw wire format and typed controller data

#### `PacsControllerDevice`

Purpose: represent one downstream I2C PACS controller at one fixed address such as `0x30`.

Suggested API:

- `bool is_online() const`
- `uint8_t address() const`
- `uint8_t interface_index() const`
- `ControllerIdentity read_identity()`
- `ControllerHealth read_health()`
- `ControllerBuildInfo read_build_info()`
- `ControllerRuntimeConfig read_runtime_config()`
- `bool upload_runtime_config(const ControllerRuntimeConfig& config)`
- `bool restart_controller()`
- `bool enter_bootloader()`
- `DoorCommandResult send_access_grant(const AccessGrantRequest& request)`
- `DoorCommandResult send_access_deny(const AccessDenyRequest& request)`
- `DoorCommandResult send_wiegand_credential(const WiegandCredentialRequest& request)`
- `OutputState read_outputs()`
- `bool set_outputs(const OutputCommand& command)`
- `bool pulse_outputs(const OutputPulseCommand& command)`
- `RgbState read_rgb()`
- `bool set_rgb(const RgbCommand& command)`
- `bool pulse_rgb(const RgbPulseCommand& command)`
- `ControllerEvent read_next_event()`

Responsibilities:

- expose one consistent high-level API per downstream controller
- hide I2C/protocol details from MQTT and business-logic layers
- enforce controller-local invariants

#### `PacsControllerManager`

Purpose: own the fleet of up to 8 downstream controllers and provide stable lookup by interface number or I2C address.

Suggested API:

- `PacsControllerDevice* find_by_interface(uint8_t access_port)`
- `PacsControllerDevice* find_by_address(uint8_t address)`
- `void scan_bus()`
- `ControllerPresenceMap presence() const`
- `void poll_events()`
- `void poll_health()`
- `void refresh_inventory()`

Responsibilities:

- fixed mapping of interface `0..7` to `0x30..0x37`
- online/offline tracking
- fleet-wide polling and status refresh
- central access point for downstream devices

#### `AccessRequestRouter`

Purpose: translate incoming MQTT messages into normalized controller actions.

Suggested API:

- `RouteResult handle_ident_message(const MqttMessage& message)`
- `RouteResult handle_access_message(const MqttMessage& message)`
- `AccessGrantRequest build_grant_request(const ParsedAccessEvent& event)`
- `AccessDenyRequest build_deny_request(const ParsedAccessEvent& event)`
- `bool should_log_to_access(const ParsedAccessEvent& event) const`

Responsibilities:

- topic-family branching
- payload validation
- `accessPort` to controller lookup
- conversion from upstream business event to downstream controller command

#### `WiegandPolicyEngine`

Purpose: decide whether a credential can be sent to a given controller and in what format.

Suggested API:

- `ValidationResult validate_ident_request(const ParsedAccessEvent& event)`
- `ValidationResult validate_access_request(const ParsedAccessEvent& event)`
- `WiegandCredentialRequest build_credential(const ParsedAccessEvent& event, const ControllerProfile& profile)`
- `ControllerProfile select_profile(uint8_t access_port)`

Responsibilities:

- Wiegand format policy
- facility-code/card-number rules
- per-controller formatting differences
- keeping Wiegand decision logic out of the I2C devices

#### `AccessEventLogger`

Purpose: publish or record monitoring-side events without coupling that behavior to controller classes.

Suggested API:

- `void log_ident_received(const ParsedAccessEvent& event)`
- `void log_ident_forwarded_to_access(const ParsedAccessEvent& event)`
- `void log_controller_result(const ParsedAccessEvent& event, const DoorCommandResult& result)`
- `void log_controller_fault(uint8_t access_port, const ControllerHealth& health)`

Responsibilities:

- never reply back to `ident`
- emit monitoring-side records into the `access` path
- centralize audit logging

### Data-model direction

To keep all 8 controllers uniform, shared data types should be defined once and reused across the entire stack.

- `ParsedAccessEvent`
- `AccessGrantRequest`
- `AccessDenyRequest`
- `WiegandCredentialRequest`
- `DoorCommandResult`
- `ControllerIdentity`
- `ControllerBuildInfo`
- `ControllerRuntimeConfig`
- `ControllerHealth`
- `ControllerEvent`
- `ControllerProfile`
- `OutputCommand`
- `OutputPulseCommand`
- `OutputState`
- `RgbCommand`
- `RgbPulseCommand`
- `RgbState`
- `ValidationResult`
- `RouteResult`

### PACS controller scope

Each downstream I2C node may grow into a full PACS door controller. The gateway design should assume the downstream feature set may include:

- Wiegand credential output
- door strike or lock relay control
- door position monitoring
- request-to-exit input
- reader beeper and LED control
- supervised input handling
- controller health and tamper reporting
- local event queue readout
- runtime configuration upload and readback

The class boundaries above should leave room for all of these without forcing a redesign later.

### Reference project lessons

Reference reviewed: `E:\Development\mqtt_wiegand_rpi4_pro_mini`

That project is worth using as a behavior reference because it already proves several important ideas in a real deployment, even though it is intentionally limited to one device at `0x30`.

What is worth carrying forward:

- keep Wiegand format building and parity generation on the host side
- keep MQTT topic parsing and business-rule decisions on the host side
- keep the downstream I2C device protocol isolated behind an adapter/protocol layer
- keep shared topic parsing as a typed model instead of string handling scattered through the code
- keep a queue or decoupled dispatch model between MQTT receipt and controller actions
- keep audit logging centralized, especially for `ident` to `access` routing behavior
- keep per-controller Wiegand configuration data-driven rather than hardcoded

What should not be copied directly:

- single-device acceptance logic that ignores `accessPort`
- the assumption that only address `0x30` exists
- Raspberry Pi specific GPIO ownership and Linux service structure
- Pro Mini specific packet names and constraints as if they were the final multi-device PACS abstraction
- project structure shaped around one host plus one constrained microcontroller

Behavioral rules confirmed by the reference project:

- inbound MQTT subscribe is destination-addressed
- `roc_access_request` from `ident` is published out on the `access` domain
- Wiegand output is prepared on the host using configured format and facility-code rules
- the downstream I2C device is treated as a transport endpoint, not the owner of access policy

Design conclusion:

- use the single-device project as a source of protocol knowledge, payload handling behavior, Wiegand policy ideas, and logging expectations
- do not use it as the direct architecture template for this firmware
- this firmware should preserve the same host-side policy ownership, but promote the device model from one fixed `0x30` adapter to a uniform fleet of 8 PACS controller objects mapped to `0x30..0x37`

### Code-organization rule

- Keep access-control code grouped by responsibility.
- Do not scatter topic parsing, Wiegand validation, and I2C packet handling across unrelated files.
- Prefer shared classes and clear module boundaries so that future rule changes only need to be implemented once.
