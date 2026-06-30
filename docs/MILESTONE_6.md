# Milestone 6 Notes

This milestone captures the next planned seam after the live I2C protocol bring-up: generic Wiegand-frame generation in the gateway before MQTT is connected to downstream `0x20` sends.

## Goal

Build a reusable C++ Wiegand builder that uses:

- `config/config.json` for per-address output format and facility code
- `config/wiegand.json` for masks, bit lengths, and parity definitions

The builder should generate the final bitstring and packed bytes needed by command `0x20`.

## Why This Is Next

The gateway has already verified:

- `0x78` build-info reads
- `0x79` event reads
- `0x21` Wiegand-out status reads
- `0x20` Wiegand-out send with a fixed test frame

The missing layer is no longer I2C transport. The missing layer is format-aware credential generation.

## Current Config Direction

Current local config now aligns the project around output Wiegand definitions:

- `config/config.json`
  - `wiegand_settings.wiegand_output_format.addr_0x30 = wiegand_48`
  - `wiegand_settings.wiegand_output_format.addr_0x31 = wiegand_48`
  - `wiegand_settings.wiegand_output_facility_codes.addr_0x30 = 8361`
  - `wiegand_settings.wiegand_output_facility_codes.addr_0x31 = 8361`
- `config/wiegand.json`
  - defines `wiegand_48`
  - defines facility-card masks
  - defines parity masks and parity types

The card number is a runtime input and should not be hard-coded in the format definition.

## Planned Class Direction

Recommended classes for this milestone:

- `WiegandFormatRepository`
  - load and expose format definitions from `config/wiegand.json`
- `WiegandOutputProfile`
  - resolve per-address output format and facility code from `config/config.json`
- `WiegandBuilder`
  - input: format name, facility code, card number
  - output: bitstring and packed bytes

These classes should be generic enough to support future additional formats without rewriting the send path.

## First Implementation Slice

The first slice should stay narrow:

1. support parsing the output profile for `addr_0x30`
2. support parsing `wiegand_48`
3. port the relevant mask/parity logic from `tools/rpi/wiegand_format.py` into C++
4. build one real 48-bit bitstring from:
   - format `wiegand_48`
   - facility code `8361`
   - one chosen test card number
5. send that generated frame with `0x20`
6. verify with:
   - Wiegand sniffer output
   - `0x21` busy/status polling
   - optional `0x79` event reads

## Rules For This Milestone

- do not connect MQTT to the downstream `0x20` path yet
- do not keep the hard-coded `10101010` frame once the real builder is proven
- do not rewrite the Python helper line-by-line; port only the relevant builder behavior into firmware-friendly C++
- keep the implementation generic enough that more output formats can be added later through config

## Done Condition

Milestone 6 is done when:

- the gateway generates a real `wiegand_48` frame from local config and format files
- the generated frame is sent successfully over `0x20`
- the sniffer confirms the expected 48-bit output
- the code structure is ready to accept other configured formats later
