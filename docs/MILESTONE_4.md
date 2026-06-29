# Milestone 4 Notes

This repository now includes a Milestone 4 MQTT path on top of the working Ethernet bring-up from Milestone 3.

## Current Milestone 4 Scope

Implemented in the current codebase:

- MQTT TCP client connection using the WIZnet Paho-style MQTT library
- config-driven broker IP, port, client ID prefix, credentials, keepalive, and subscribe topics
- startup subscribe to the configured SPV1.0 `ident` and `access` topics
- startup publish of a simple online/test status message
- startup publish to each configured monitor/server in `publish_to_servers`
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
  "subscribe_topics": [
    "SPV1.0/ident/+/+/<controller_serial>",
    "SPV1.0/access/+/+/<controller_serial>"
  ],
  "publish_to_servers": {
    "51918423475411": "ROCWatch & Monitor_64",
    "256508198617407": "ROCWatch & Monitor_133"
  }
}
```

Notes:

- `broker` is currently treated as an IPv4 literal, not a hostname.
- `publish_to_servers` keys are used as the source-side identifiers for `access` response topics.
- `subscribe_topics` is a runtime template list.
- `<controller_serial>` is replaced at startup with the controller's MAC-derived decimal serial
  number.

## Important Current Behavior

- On successful boot, the firmware publishes:
  - `SPV1.0/access/stc_online_status_response/<publish_to_servers key>/<controller_serial>`
  - payload fields include `_iD`, `clientId`, `programVersion`, `lastUpdated`, `git_number`,
    `controller_name`, `hostName`, `ipAddress`, `dateTime`, `unixTime`, `response`, and `reason`
- The controller subscribes to:
  - each configured controller-addressed subscription topic after `<controller_serial>` expansion
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
