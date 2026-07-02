# Next Milestones

This document translates the original milestone plan in [CODEX_SPEC.md](CODEX_SPEC.md) into the next practical implementation steps for the current repository state.

## Current Baseline

The codebase has already progressed beyond the original Milestone 5 bring-up notes in a few areas:

- DS2401 startup identity read is implemented on `GP6`
- DS2401 ROM CRC is validated before the value is accepted
- DS2401 ROM reads now retry on failure
- runtime MAC can be overridden from DS2401 before Ethernet startup
- startup serial output can be gated by a push-button on `GP9`
- Ethernet startup prints the active IP address
- LittleFS is used as the active onboard config store with factory fallback

The design direction for the access-control path is documented in [ACCESS_CONTROL_DESIGN.md](ACCESS_CONTROL_DESIGN.md).

## Recommended Next Milestone Order

### Milestone 6

Time base and persistent time support.

Goals:

- implement RTC manager class and move RTC handling out of the generic I2C manager
- add NTP client support on top of the working Ethernet path
- update RTC from NTP during startup when possible
- use UTC internally and local timezone only for display/log formatting

Definition of done:

- RTC is initialized through a dedicated manager
- NTP success or failure is reported clearly at startup
- if NTP succeeds, RTC is updated
- if NTP fails, firmware continues using RTC without stopping the system

### Milestone 7

Access-request parsing and first real MQTT-to-I2C access transaction path.

Goals:

- define shared data models for parsed MQTT access events
- implement centralized SPV1.0 message parsing for `ident` and `access`
- validate destination identity and `mqtt_target`
- map `accessPort` to interface `0..7` and I2C address `0x30..0x37`
- load and apply Wiegand format policy from config and format definitions
- send a real downstream I2C Wiegand/output command to one controller path
- publish a structured transaction result to the monitoring side

Definition of done:

- one end-to-end access request can be received from MQTT
- policy and format are decided in the gateway
- one controller command is emitted over I2C
- a structured result is published on the `access` domain

### Milestone 8

Multi-device PACS controller abstraction and event polling.

Goals:

- implement `I2cDeviceTransport`
- implement `Rp2350PacsProtocol`
- implement `PacsControllerDevice`
- implement `PacsControllerManager`
- track controller presence for addresses `0x30..0x37`
- support consistent per-device operations through shared classes

Definition of done:

- the application no longer handles per-device access logic with ad hoc address checks
- all downstream controllers are represented through one common API
- offline or missing devices are handled cleanly without stopping the system

### Milestone 9

IRQ or event-driven downstream message handling.

Goals:

- confirm the actual downstream message/event protocol for the RP2350 I2C devices
- implement event polling or IRQ handling per controller
- route controller events into shared event structures
- publish meaningful monitoring-side event/status messages

Definition of done:

- controller-side events can be detected and decoded
- event handling is separated from request-routing logic
- event output is normalized across all controller interfaces

### Milestone 10

Linked interface transaction completion and RS485 integration.

Goals:

- implement the RS485 manager with retry and timeout handling
- define how I2C and RS485 actions combine per interface
- complete the full logical-interface transaction path
- define partial-success and retry behavior clearly

Definition of done:

- one logical interface can coordinate both I2C and RS485 work
- transaction results clearly report I2C and RS485 outcomes
- retry behavior is explicit and documented

## Cross-Cutting Documentation Work

These items should be kept current while milestones move forward:

- README remains a high-level index and current-status summary
- design-heavy rules live under `docs/`
- milestone notes should be refreshed whenever the real code moves materially beyond the last documented state
- git history in the README should be updated after important checkpoints

## Open Decisions Blocking Later Milestones

These should be resolved before or during Milestones 7 through 10:

- final MQTT payload schema for access transactions
- final result-publish payload schema
- exact RP2350 I2C command set to use first for PACS control
- exact IRQ or event-read behavior for the downstream RP2350 devices
- exact RS485 protocol and transaction rules
- whether raw-bit MQTT requests are supported, or only normalized credential requests
- whether success requires I2C only, RS485 only, or both
