# Milestone 3 Notes

This repository now includes a Milestone 3 Ethernet bring-up path on top of the working SD-backed configuration flow from Milestone 2.

## Current Milestone 3 Scope

Implemented in the current codebase:

- W6300 transport initialization using the official WIZnet Pico library sources
- Ethernet startup owned by `src/network/EthernetManager.*`
- DHCP or static IPv4 selection from `config.json`
- MAC address application from `device.mac`
- serial printout of the active network configuration and IP address

Not yet implemented in this milestone slice:

- runtime reconfiguration without reboot
- richer network diagnostics beyond the WIZnet summary output
- LCD display of IP information
- MQTT traffic over the new Ethernet connection

## Architecture Notes

- `src/core/App.*` now initializes LED, SD card, config loading, and Ethernet in startup order.
- `src/config/RuntimeConfig.h` now carries the minimum Ethernet fields needed for DHCP or static IPv4 startup.
- `src/config/ConfigManager.*` now accepts:
  - `ethernet.mode`
  - `ethernet.static.ip`
  - `ethernet.static.subnet`
  - `ethernet.static.gateway`
  - `ethernet.static.dns`
- `src/network/EthernetManager.*` owns W6300 reset, transport setup, DHCP/static selection, and periodic DHCP servicing.

## Important Current Behavior

- `ethernet.mode = "dhcp"` blocks during boot until a lease is acquired or a timeout occurs.
- `ethernet.mode = "static"` applies the configured IPv4 values immediately and prints the resulting network summary.
- The W6300 board-side transport pins come from the official WIZnet W6300-EVB-Pico2 support files, not from the SD adapter SPI pins.
- SD card SPI remains on `SPI1` with `GP10-13` and is separate from Ethernet bring-up.

## Expected `config.json` Shape

The Ethernet section now supports this minimal shape:

```json
"ethernet": {
  "mode": "dhcp",
  "static": {
    "ip": "192.168.1.60",
    "subnet": "255.255.255.0",
    "gateway": "192.168.1.1",
    "dns": "192.168.1.1"
  }
}
```

Notes:

- When `mode` is `dhcp`, the `static` block may still exist but is ignored.
- When `mode` is `static`, all four `static` fields are required by the current parser.

## What To Test Right Now

With the current firmware loaded, serial output should show:

1. Milestone 3 boot banner
2. successful SD mount and config parse
3. `Ethernet: initializing W6300 transport`
4. either `Ethernet: DHCP mode requested` or `Ethernet: static IP mode requested`
5. printed network summary including MAC and IP address
6. continuing LED heartbeat in the main loop

## Next Planned Seam

After Milestone 3 is proven on hardware, the next step should be:

1. keep the network path stable across reboots
2. verify DHCP renewal behavior during long uptime
3. begin MQTT connect/publish/subscribe work on top of the new Ethernet manager
