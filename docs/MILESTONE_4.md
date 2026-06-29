# Milestone 4 Notes

This repository now includes a Milestone 4 MQTT path on top of the working Ethernet bring-up from Milestone 3.

## Current Milestone 4 Scope

Implemented in the current codebase:

- MQTT TCP client connection using the WIZnet Paho-style MQTT library
- config-driven broker IP, port, client ID prefix, credentials, keepalive, and subscribe topic
- startup subscribe to the configured SPV1.0 access topic
- optional subscribe to the SPV1.0 online-status discovery response topic
- startup publish of a simple online/test status message
- basic SPV1.0 topic parsing with serial printout of parsed fields

Not yet implemented in this milestone slice:

- payload-level access request processing
- monitor discovery state storage
- reconnect and backoff strategy
- transaction result publishing for real access operations

## Architecture Notes

- `src/mqtt/MqttManager.*` owns MQTT connect, subscribe, publish, timer servicing, and topic parsing.
- `src/core/App.*` now initializes MQTT after Ethernet is up.
- `src/config/RuntimeConfig.h` and `src/config/ConfigManager.*` now carry the additional MQTT fields needed by this milestone.
- The controller serial number is derived from the configured MAC address, following the spec rule that the MAC-derived decimal value is used for MQTT identity and SPV1.0 IDs.

## Expected `config.json` MQTT Shape

The current firmware expects:

```json
"mqtt": {
  "broker": "192.168.1.122",
  "port": 1883,
  "client_id_prefix": "w6300",
  "username": "",
  "password": "",
  "keep_alive_sec": 60,
  "discovery_enabled": true,
  "broadcast_destination_id": "0",
  "subscribe_topic": "SPV1.0/+/+/+/<controller_serial>"
}
```

Notes:

- `broker` is currently treated as an IPv4 literal, not a hostname.
- `broadcast_destination_id` is used when the startup status topic is published.
- `subscribe_topic` is a runtime template. `<controller_serial>` is replaced at startup with the
  controller's MAC-derived decimal serial number.

## Important Current Behavior

- On successful boot, the firmware publishes:
  - `SPV1.0/system/stc_online_status_request/<controller_serial>/<broadcast_destination_id>`
- The controller also subscribes to:
  - the configured controller-addressed subscription topic after `<controller_serial>` expansion
  - the discovery-response topic when `discovery_enabled` is `true`
- Received MQTT messages are not executed yet; their topics and payloads are printed and the SPV1.0 topic is parsed for verification.
- The firmware now expects the port/interface value to be carried in the payload rather than as a topic segment.

## What To Test Right Now

With the current firmware loaded, serial output should show:

1. Milestone 4 boot banner
2. successful SD and Ethernet startup
3. `MQTT: connected ...`
4. subscription confirmation lines
5. startup status publish confirmation
6. parsed fields when a matching MQTT message arrives

## Next Planned Seam

After Milestone 4 is proven on hardware, the next step should be:

1. parse access-request payloads
2. validate destination IDs against the controller serial
3. store monitor discovery state
4. publish structured response topics for completed operations
