# CODEX_SPEC.md

# W6300-EVB-Pico2 Access Gateway Firmware Specification

## 1. Purpose

This document is the source specification for Codex to implement the firmware for a WIZnet W6300-EVB-Pico2 based access-control gateway.

The firmware shall be written in C++ using Pico SDK and WIZnet W6300 libraries.

Codex must read this document before writing or modifying code.

---

## 2. High-Level Goal

Create firmware for a W6300-EVB-Pico2 controller that:

- Reads configuration from an SD card at startup.
- Reads Wiegand format definitions from the SD card.
- Configures Ethernet using DHCP or static IP from `config.json`.
- Connects to a Mosquitto MQTT broker.
- Uses the SPV1.0 MQTT topic protocol.
- Dynamically discovers the master monitor at startup.
- Receives MQTT access commands.
- Converts MQTT commands and payloads into Wiegand output commands.
- Talks to 8 linked Wiegand processor interfaces.
- Talks to 8 linked RS485 devices.
- Publishes transaction results to a monitor listener.
- Uses NTP to update a local RTC at startup.
- Uses the RTC as the local time source after boot.
- Displays configurable status information on a 4x20 I2C LCD.
- Uses a status LED heartbeat: 100 ms ON and 900 ms OFF when healthy.
- Supports remote support commands over MQTT for SD card file retrieval and safe config updates.

---

## 3. Main Hardware

### Main Controller

- Board: WIZnet W6300-EVB-Pico2
- MCU: RP2350
- Ethernet: WIZnet W6300
- Language: C++
- Framework: Pico SDK
- Ethernet library direction: WIZnet-PICO-C and WIZnet ioLibrary_Driver

### Peripherals

- SD card for configuration, Wiegand formats, logs, queue files, support files.
- I2C RTC, likely DS3231 at address `0x68`.
- I2C 4x20 LCD, likely address `0x27` or `0x3F`.
- I2C Wiegand processors at `0x30` to `0x37`.
- 8 dedicated IRQ input pins from the Wiegand processors.
- UART RS485 bus using a 75176-type transceiver with RTS/DE direction control.
- 8 RS485 addressed devices, linked to the same logical interface numbers as the Wiegand processors.
- Status LED.
- 12 V PoE-derived system rail.
- Per-Wiegand-module 5 V switch-mode power supplies.
- Per-Wiegand-module low-voltage controller / reset supervisor.

---

## 4. Power Architecture

The system power architecture shall use a 12 V PoE-derived main rail.

Recommended structure:

```text
PoE module -> 12 V main rail
  ├── W6300-Pico2 local regulator
  ├── RS485 / support circuitry
  ├── Wiegand module 0 local 5 V buck regulator
  ├── Wiegand module 1 local 5 V buck regulator
  ├── Wiegand module 2 local 5 V buck regulator
  ├── Wiegand module 3 local 5 V buck regulator
  ├── Wiegand module 4 local 5 V buck regulator
  ├── Wiegand module 5 local 5 V buck regulator
  ├── Wiegand module 6 local 5 V buck regulator
  └── Wiegand module 7 local 5 V buck regulator
```

Each Wiegand processor module shall have:

- Local 12 V input protection.
- Local 5 V buck converter.
- Recommended 5 V / 1 A maximum capability.
- Low-voltage controller / reset supervisor.
- Field protection.
- Diode isolation or equivalent protection on pull-ups to prevent external third-party devices from back-powering the unit.

### Suggested 5 V Buck Converter Numbers

Per Wiegand module:

```text
Input voltage:        12 V nominal
Input range:          9 V to 15 V minimum
Output voltage:       5.0 V
Output current:       1.0 A max
Output power:         5 W max
Efficiency target:    85% or better
Input current @12 V:  ~0.49 A at full load
Input current @9 V:   ~0.65 A at full load
```

Recommended manufacturers for production-quality switch-mode regulators:

- RECOM
- TRACO Power
- Murata
- Texas Instruments
- Monolithic Power Systems
- Analog Devices
- Mean Well

Preferred production direction: use RECOM or TRACO SIP-style 5 V / 1 A regulators for reliability and simpler PCB layout.

---

## 5. Modular Wiegand Processor Hardware

Each Wiegand processor shall be implemented as a modular daughterboard.

Each module represents one linked access interface and provides:

- I2C communication to the main board.
- IRQ output to the main board.
- Wiegand input/output field wiring.
- Local 5 V buck PSU.
- Local LVC/reset supervisor.
- Wiegand field protection.
- Optional local RGB/status LED.

Design rule:

```text
One faulty Wiegand module must not be able to take down the entire system.
```

Each module should be replaceable for field maintenance.

---

## 6. Motherboard and Extension Board

One motherboard houses the main controller and Wiegand processors 0 to 3.

A second extension board houses Wiegand processors 4 to 7.

```text
Motherboard:
  Interface 0 -> I2C 0x30 / IRQ0
  Interface 1 -> I2C 0x31 / IRQ1
  Interface 2 -> I2C 0x32 / IRQ2
  Interface 3 -> I2C 0x33 / IRQ3

Extension board:
  Interface 4 -> I2C 0x34 / IRQ4
  Interface 5 -> I2C 0x35 / IRQ5
  Interface 6 -> I2C 0x36 / IRQ6
  Interface 7 -> I2C 0x37 / IRQ7
```

### 10-Pin Ribbon Cable Between Motherboard and Extension Board

The expansion board shall connect using a 10-pin ribbon cable.

Final pinout:

```text
1   +12V
2   +12V
3   GND
4   IRQ4
5   IRQ5
6   IRQ6
7   IRQ7
8   GND
9   I2C_SDA
10  I2C_SCL
```

Recommended hardware details:

- Keyed IDC connector.
- Clear pin 1 marking.
- Ribbon cable kept short and inside the enclosure.
- Bulk capacitance on the extension board.
- TVS and current protection on the 12 V feed.
- Pull-ups for SDA/SCL should be controlled carefully, preferably only on the motherboard unless testing proves otherwise.
- Optional series resistors on SDA/SCL.
- Optional filtering/debounce on IRQ lines.

If the extension board is missing, interfaces 4 to 7 shall be marked unavailable and interfaces 0 to 3 shall continue operating.

---

## 7. Logical Interface Model

The system has 8 logical interfaces.

Each logical interface links one Wiegand processor and one RS485 device.

```text
Interface 0 -> I2C 0x30 + RS485 device 0
Interface 1 -> I2C 0x31 + RS485 device 1
Interface 2 -> I2C 0x32 + RS485 device 2
Interface 3 -> I2C 0x33 + RS485 device 3
Interface 4 -> I2C 0x34 + RS485 device 4
Interface 5 -> I2C 0x35 + RS485 device 5
Interface 6 -> I2C 0x36 + RS485 device 6
Interface 7 -> I2C 0x37 + RS485 device 7
```

Important rule:

```text
The SPV1.0 topic interface field selects the logical access interface.
The firmware maps that interface to the physical I2C and RS485 devices.
```

---

## 8. Recommended Source Structure

Do not write one giant `main.cpp`.

Recommended structure:

```text
src/
  main.cpp

  core/
    App.cpp / App.h
    SystemStateManager.cpp / .h
    EventQueue.cpp / .h
    WatchdogManager.cpp / .h
    Logger.cpp / .h

  config/
    ConfigManager.cpp / .h
    JsonParser.cpp / .h

  storage/
    SdCardManager.cpp / .h
    FileManager.cpp / .h

  network/
    EthernetManager.cpp / .h
    MqttManager.cpp / .h
    NtpManager.cpp / .h

  protocol/
    SPV1Topic.cpp / .h
    JsonMessage.cpp / .h
    TransactionMessage.cpp / .h

  access/
    InterfaceManager.cpp / .h
    WiegandFormatManager.cpp / .h
    WiegandBuilder.cpp / .h
    WiegandIrqManager.cpp / .h

  bus/
    I2CManager.cpp / .h
    PiWiegandI2CDevice.cpp / .h
    RS485Manager.cpp / .h
    RocEmRsDevice.cpp / .h

  devices/
    RTCManager.cpp / .h
    LcdManager.cpp / .h
    LedManager.cpp / .h
```

Each manager should generally expose:

```text
init()
service()
status()
last_error()
```

---

## 9. Startup Sequence

Startup order:

```text
1. Start serial debug.
2. Initialize status LED.
3. Mount SD card.
4. Read /config.json.
5. Read /wiegand_formats.json.
6. Validate configuration.
7. Initialize I2C.
8. Initialize LCD.
9. Initialize RTC.
10. Configure Wiegand IRQ input pins.
11. Scan I2C processors 0x30 to 0x37 using VER/JMP where applicable.
12. Initialize RS485 UART and RTS/DE control pin.
13. Start W6300 Ethernet.
14. Apply DHCP or static addressing.
15. Start NTP client.
16. If NTP succeeds, update RTC.
17. If NTP fails, continue using RTC and publish warning later.
18. Connect to MQTT broker.
19. Subscribe to startup/discovery topics.
20. Publish online broadcast discovery message.
21. Receive monitor assignment.
22. Subscribe to operational command topics.
23. Publish online/status response.
24. Enter main event loop.
```

---

## 10. Runtime Loop

Avoid long blocking delays.

Runtime loop concept:

```text
while true:
  service LED timing
  service Ethernet link status
  service MQTT
  service MQTT discovery/monitor assignment
  service I2C queue
  service Wiegand IRQ events
  service RS485 queue
  service LCD updates
  service RTC/time
  service SD/log queues
  service watchdog
  process event queue
```

The firmware shall be event-driven as much as possible.

---

## 11. Device Identity

The Ethernet MAC address shall be used as the unique device serial number.

```text
MAC address -> decimal serial number
```

Example:

```text
MAC: 02:63:00:12:34:56
HEX: 026300123456
DEC: used as serial_number
```

The decimal MAC-derived serial number shall be used for:

- MQTT client ID suffix.
- SPV1.0 source ID.
- SPV1.0 destination ID.
- LCD display.
- Logs.
- Transaction messages.

MAC addresses shall be generated once and stored permanently.

Use a locally administered MAC range:

```text
02:xx:xx:xx:xx:xx
```

Do not regenerate the MAC automatically on every boot.

---

## 12. SD Card Files

Minimum required files:

```text
/config.json
/wiegand_formats.json
```

Recommended future/optional files:

```text
/mac_address.txt
/logs/system.log
/logs/transactions.log
/queue/offline_transactions/
/backup/config_001.json
/backup/config_002.json
/backup/config_003.json
/tmp/
```

If the SD card or required configuration files are missing, the controller shall enter safe mode.

---

## 13. config.json Draft

This is a draft schema. Codex should use it as the starting point.

```json
{
  "config_version": 1,

  "device": {
    "name": "w6300-access-gateway",
    "mac": "02:63:00:12:34:56"
  },

  "ethernet": {
    "mode": "dhcp",
    "static": {
      "ip": "192.168.1.60",
      "subnet": "255.255.255.0",
      "gateway": "192.168.1.1",
      "dns": "192.168.1.1"
    }
  },

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
  },

  "time": {
    "timezone": "America/New_York",
    "rtc": {
      "enabled": true,
      "type": "DS3231",
      "address": "0x68"
    },
    "ntp": {
      "enabled": true,
      "server": "pool.ntp.org",
      "timeout_ms": 5000,
      "update_rtc_on_boot": true,
      "update_interval_hours": 24
    }
  },

  "i2c": {
    "enabled": true,
    "bus": 0,
    "sda_pin": 4,
    "scl_pin": 5,
    "frequency": 100000,
    "wiegand_base_address": "0x30",
    "interface_count": 8
  },

  "wiegand_irq": {
    "enabled": true,
    "active_level": "low",
    "edge": "falling",
    "debounce_ms": 5,
    "pins": [
      { "interface": 0, "pin": 14 },
      { "interface": 1, "pin": 15 },
      { "interface": 2, "pin": 16 },
      { "interface": 3, "pin": 2 },
      { "interface": 4, "pin": 3 },
      { "interface": 5, "pin": 7 },
      { "interface": 6, "pin": 8 },
      { "interface": 7, "pin": 9 }
    ]
  },

  "rs485": {
    "enabled": true,
    "uart": 0,
    "baud": 9600,
    "tx_pin": 0,
    "rx_pin": 1,
    "rts_pin": 2,
    "timeout_ms": 100,
    "retry_count": 3,
    "retry_delay_ms": 50,
    "crc_enabled": true,
    "interface_count": 8
  },

  "interfaces": [
    {
      "interface": 0,
      "name": "Door 0",
      "i2c_address": "0x30",
      "rs485_address": 0,
      "allowed_in_facility_codes": [246],
      "out_facility_code": 246,
      "default_wiegand_format": "wiegand_48"
    },
    {
      "interface": 1,
      "name": "Door 1",
      "i2c_address": "0x31",
      "rs485_address": 1,
      "allowed_in_facility_codes": [246],
      "out_facility_code": 246,
      "default_wiegand_format": "wiegand_48"
    }
  ],

  "lcd": {
    "enabled": true,
    "type": "i2c_20x4",
    "address": "0x27",
    "columns": 20,
    "rows": 4,
    "refresh_ms": 500,
    "lines": [
      {
        "line": 0,
        "mode": "static",
        "text": "W6300 Gateway"
      },
      {
        "line": 1,
        "mode": "alternate",
        "interval_sec": 10,
        "items": [
          "SN: {serial_number}",
          "IP: {ip_address}"
        ]
      },
      {
        "line": 2,
        "mode": "status",
        "text": "MQTT: {mqtt_status}"
      },
      {
        "line": 3,
        "mode": "last_event",
        "text": "IF{interface}: {result}"
      }
    ]
  },

  "led": {
    "healthy_on_ms": 100,
    "healthy_off_ms": 900
  },

  "logging": {
    "enabled": true,
    "level": "INFO",
    "system_log": "/logs/system.log",
    "transaction_log": "/logs/transactions.log"
  },

  "remote_support": {
    "enabled": true,
    "allow_sdcard_read": true,
    "allow_config_upload": true,
    "max_mqtt_payload_bytes": 1024,
    "allowed_paths": [
      "/config.json",
      "/wiegand_formats.json",
      "/logs/",
      "/stats/"
    ]
  }
}
```

Some pins in this draft are now reserved or confirmed in the current project documentation, while others remain placeholders until final hardware pinout is confirmed.

---

## 14. wiegand_formats.json

Wiegand formats shall not be hard-coded in firmware.

They shall be read from `/wiegand_formats.json`.

Example draft:

```json
{
  "wiegand_26": {
    "bit_len": 26,
    "facility_code_mask": "00000000111111110000000000",
    "card_number_mask":   "00000000000000001111111111111110",
    "parity": [
      {
        "bit": 0,
        "type": "even",
        "mask": "01111111111110000000000000"
      },
      {
        "bit": 25,
        "type": "odd",
        "mask": "00000000000001111111111110"
      }
    ]
  },

  "wiegand_48": {
    "bit_len": 48,
    "facility_code_mask": "",
    "card_number_mask": "",
    "parity": []
  }
}
```

Codex must implement Wiegand building and parsing generically using these format definitions.

---

## 15. Facility Code Rules

Facility codes are configured per logical interface.

Each interface has:

```text
allowed_in_facility_codes = list of accepted incoming facility codes
out_facility_code = one fixed facility code used for outgoing Wiegand
```

Incoming Wiegand rule:

```text
Read Wiegand input from processor.
Decode facility code.
Check whether facility code exists in allowed_in_facility_codes for that interface.
If allowed, publish accepted event.
If not allowed, publish rejected event.
```

Outgoing Wiegand rule:

```text
MQTT payload provides credential/card number.
Firmware gets out_facility_code from config.json for that interface.
Firmware builds Wiegand bitstream using out_facility_code + card number + selected Wiegand format.
Firmware sends WIE,<addr>,<bitcount>,<bitstring> to the Wiegand processor.
```

Important rule:

```text
The MQTT sender shall not choose the outgoing facility code.
The outgoing facility code is controlled locally by config.json.
```

---

## 16. SPV1.0 MQTT Topic Format

Updated SPV1.0 topic format:

```text
SPV1.0/<domain>/<command>/<source_id>/<destination_id>
```

Example:

```text
SPV1.0/access/stc_access_request/256508198617407/281261212083555
```

Parsed as:

```text
sp_version     = SPV1.0
domain         = access
command        = stc_access_request
source_id      = 256508198617407
destination_id = 281261212083555
```

Important rule:

```text
The linked physical access interface is carried in the payload, not in the topic path.
```

`destination_id = 0` means broadcast/system discovery.

---

## 17. MQTT Dynamic Monitor Discovery

During startup, the controller shall dynamically discover its monitor/master destination ID.

Startup discovery flow:

```text
Controller boots.
Controller connects to MQTT broker.
Controller publishes online broadcast discovery request.
Monitor/master receives broadcast.
Monitor/master responds directly to the controller.
Controller validates response.
Controller stores monitor serial number in RAM.
Controller publishes future transaction results to the monitor.
```

Controller broadcast topic:

```text
SPV1.0/system/stc_online_status_request/<controller_serial>/0
```

Monitor response topic:

```text
SPV1.0/system/stc_online_status_response/<monitor_serial>/<controller_serial>
```

Monitor response payload example:

```json
{
  "_iD": "abc123",
  "monitor_serial": "281261212083555",
  "controller_serial": "256508198617407",
  "controller_name": "w6300-gateway-01",
  "mqtt_keepalive_sec": 60,
  "timestamp_utc": "2026-05-16T12:00:00Z"
}
```

Controller behavior:

```text
1. Send online discovery broadcast.
2. Wait for response.
3. Validate response destination matches controller serial.
4. Store monitor_serial as runtime monitor destination ID.
5. Publish future transactions to that monitor_serial.
```

---

## 18. MQTT Access Request Flow

```text
MQTT access request received
  ↓
Parse SPV1.0 topic
  ↓
Validate destination_id matches this controller serial number
  ↓
Parse JSON payload
  ↓
Extract interface number 0 to 7 from payload
  ↓
Select Wiegand format
  ↓
Use out_facility_code from config.json for that interface
  ↓
Build Wiegand bitstring
  ↓
Send Wiegand output command over I2C
  ↓
Optionally send linked RS485 command
  ↓
Collect ACK/NAK/timeout results
  ↓
Publish transaction result to monitor topic
```

---

## 19. MQTT Payloads

Access request payload example:

```json
{
  "_iD": "abc123",
  "wiegand_format": "wiegand_48",
  "card_number": 21620,
  "direction": "out"
}
```

Raw-bit payload example:

```json
{
  "_iD": "abc123",
  "bit_len": 48,
  "bitstring": "010101010101010101010101010101010101010101010101"
}
```

Codex should support both:

```text
1. facility/card + format -> build bitstring
2. raw bitstring -> send directly
```

For normal outgoing operation, the facility code comes from config.json, not from MQTT.

---

## 20. MQTT Transaction Result

Publish a transaction result after each request.

Recommended response topic:

```text
SPV1.0/access/stc_access_response/<controller_serial>/<monitor_serial>
```

Example success payload:

```json
{
  "_iD": "abc123",
  "success": true,
  "source_topic": "SPV1.0/access/stc_access_request/256508198617407/281261212083555",
  "port": 0,
  "i2c_address": "0x30",
  "rs485_address": 0,
  "wiegand_format": "wiegand_48",
  "bit_len": 48,
  "i2c_result": "ACK",
  "rs485_result": "ACK",
  "attempts": 1,
  "timestamp_utc": "2026-05-15T21:00:00Z"
}
```

Failure example:

```json
{
  "_iD": "abc123",
  "success": false,
  "interface": 0,
  "i2c_result": "TIMEOUT",
  "rs485_result": "NOT_SENT",
  "attempts": 3,
  "error": "No ACK from Wiegand processor",
  "timestamp_utc": "2026-05-15T21:00:00Z"
}
```

---

## 21. I2C Wiegand Processor Protocol

Use this existing protocol exactly as the base protocol.

Commands:

```text
JMP,<addr>
VER,<addr>
WIE,<addr>,<bitcount>,<bitstring>
RGB,<addr>,<color_index>,<timer_ms>
RST,<addr>
```

Examples:

```text
JMP,0
VER,0
WIE,0,48,010101010101010101010101010101010101010101010101
RGB,0,2,1000
RST,0
```

Expected known responses:

```text
ACK
NAK
EMP
JMP_VAL:<n>
PiWiegand: vX.Y.Z
```

The W6300-Pico2 shall not generate Wiegand pulses directly.

The Wiegand processor handles pulse timing locally.

For 48-bit Wiegand:

```text
Pulse width = 1 µs
Bit duration = 1 ms
48 bits ≈ 48.05 ms
Safe estimate = 50 ms
```

The main controller should consider a Wiegand processor busy for about 50 ms after sending a 48-bit Wiegand output, or until protocol status indicates ready.

---

## 22. Wiegand IRQ Inputs and Buffered Messages

There are 8 IRQ input pins from the Wiegand processors to the main W6300-Pico2.

IRQ mapping:

```text
IRQ0 -> Interface 0 -> I2C 0x30
IRQ1 -> Interface 1 -> I2C 0x31
IRQ2 -> Interface 2 -> I2C 0x32
IRQ3 -> Interface 3 -> I2C 0x33
IRQ4 -> Interface 4 -> I2C 0x34
IRQ5 -> Interface 5 -> I2C 0x35
IRQ6 -> Interface 6 -> I2C 0x36
IRQ7 -> Interface 7 -> I2C 0x37
```

Important rule:

```text
IRQ asserted means: this interface has one or more pending buffered messages.
IRQ does not only mean Wiegand number ready.
```

A pending message may be:

- Wiegand number captured.
- Pin edge detected.
- Status event.
- Error event.
- Tamper event.
- Other local processor event.

The GPIO ISR shall not perform I2C reads directly.

The ISR shall only queue or mark a lightweight event.

The main loop shall perform I2C reads.

Recommended message tags:

```text
WIEGAND_IN
PIN_EDGE
STATUS_EVENT
ERROR_EVENT
TAMPER_EVENT
VERSION_EVENT
```

Recommended future read command, if not already present in Wiegand processor firmware:

```text
MSG,<addr>
```

Possible response examples:

```text
MSG,WIEGAND_IN,48,010101010101...
MSG,PIN_EDGE,2,RISING
MSG,STATUS_EVENT,READY
MSG,ERROR_EVENT,BUFFER_OVERFLOW
MSG,EMPTY
```

Key design rule:

```text
IRQ = pending buffered message.
message tag = what happened.
```

---

## 23. RS485 Design

RS485 hardware:

```text
UART TX
UART RX
RTS/DE pin controlling 75176 transmit/receive direction
8 addressed RS485 slave devices
```

RS485 must include retries.

Transaction flow:

```text
Set RTS/DE to transmit.
Send packet.
Wait for UART transmission complete.
Set RTS/DE to receive.
Wait for response.
Validate response.
If timeout/CRC/NAK, retry.
After max retries, fail transaction.
Publish result.
```

Important rules:

```text
Only one RS485 transaction may be active at a time.
Every command should have a transaction ID if supported by the protocol.
The slave should echo transaction ID if supported.
Do not blindly retry if a valid BUSY response is returned.
```

Configurable values:

- UART number.
- TX pin.
- RX pin.
- RTS/DE pin.
- Baud rate.
- Timeout.
- Retry count.
- Retry delay.
- CRC enabled/disabled.

The exact `roc_em_rs` protocol still needs to be supplied before final implementation.

---

## 24. Time System

The system shall use NTP on startup to update the RTC.

Startup time behavior:

```text
Ethernet ready.
Resolve NTP server.
Get UTC time from NTP.
If NTP succeeds, update RTC.
If NTP fails, continue using RTC.
```

Architecture:

```text
NTP -> RTC -> system timestamps
```

The RTC should store UTC.

Use timezone from config only for display/local formatting.

Use IANA timezone names:

```text
America/New_York
Africa/Johannesburg
Europe/London
```

Avoid fixed offsets such as `UTC-5` because daylight saving time would be wrong.

---

## 25. LCD Requirements

LCD content must be controlled by `config.json`.

The firmware provides variables:

```text
{serial_number}
{mac_address}
{ip_address}
{mqtt_status}
{ethernet_status}
{interface}
{result}
{rtc_time}
{date}
{last_error}
```

Example normal display:

```text
Line 0: W6300 Gateway
Line 1: alternates every 10 seconds:
        SN: <serial_number>
        IP: <ip_address>
Line 2: MQTT: Connected
Line 3: IF0: ACK
```

LCD updates must be throttled and non-blocking.

---

## 26. LED Requirements

Healthy heartbeat:

```text
ON  = 100 ms
OFF = 900 ms
```

Recommended LED states:

```text
Booting             100 ms ON / 100 ms OFF
Healthy             100 ms ON / 900 ms OFF
SD/config error     50 ms pulse every 2 seconds
Ethernet down       200 ms ON / 200 ms OFF
MQTT disconnected   100 ms ON / 400 ms OFF
Fatal error         solid ON
```

Do not use blocking delays for LED timing.

---

## 27. MQTT SD Card File Access

The controller shall support MQTT commands to retrieve files or file content from the SD card.

Primary uses:

- Remote diagnostics.
- Retrieve logs.
- Retrieve config.json.
- Retrieve wiegand_formats.json.
- Retrieve statistics.
- Retrieve queued transactions.

Recommended request topic:

```text
SPV1.0/system/stc_sdcard_request/<source_id>/<destination_id>
```

Recommended response topic:

```text
SPV1.0/system/stc_sdcard_response/<controller_serial>/<monitor_serial>
```

Example read request:

```json
{
  "_iD": "abc123",
  "command": "read_file",
  "path": "/config.json"
}
```

Example list request:

```json
{
  "_iD": "abc123",
  "command": "list_directory",
  "path": "/logs"
}
```

Example tail request:

```json
{
  "_iD": "abc123",
  "command": "tail_file",
  "path": "/logs/system.log",
  "lines": 50
}
```

Large files must be chunked.

Chunked response concept:

```json
{
  "_iD": "abc123",
  "success": true,
  "command": "read_file",
  "path": "/logs/system.log",
  "chunk": 1,
  "total_chunks": 5,
  "content_base64": "..."
}
```

Do not allow unrestricted SD access.

Block:

- `../` path traversal.
- Unknown paths.
- Hidden files unless explicitly allowed.
- Firmware/private files unless explicitly allowed.

---

## 28. Remote Config Upload / Support

The controller shall support MQTT-based file upload and remote configuration update.

Supported operations:

```text
read_file
list_directory
tail_file
upload_file
replace_config
replace_wiegand_formats
delete_log
reboot_controller
validate_config
```

Upload request concept:

```json
{
  "_iD": "abc123",
  "command": "upload_file",
  "path": "/config.json",
  "chunk": 1,
  "total_chunks": 3,
  "content_base64": "....",
  "sha256": "optional-final-file-hash"
}
```

Safe update flow:

```text
Receive upload chunks.
Write to temp file, for example /tmp/config.new.
Validate chunk count and final hash/CRC.
Validate JSON syntax.
Validate required fields.
Backup current config to /backup/.
Replace active config.json.
Publish success/failure.
Optionally reboot.
```

Never overwrite active config directly.

Use:

```text
temp file -> validate -> backup -> replace
```

Keep at least 3 config backups:

```text
/backup/config_001.json
/backup/config_002.json
/backup/config_003.json
```

If config is corrupted:

```text
1. Restore latest valid backup.
2. Publish recovery event if possible.
3. Continue operation.
```

If no valid backup exists:

```text
Enter safe mode.
```

---

## 29. Error Handling

Firmware must handle:

- SD card missing.
- `config.json` missing.
- `config.json` invalid.
- `wiegand_formats.json` missing.
- Ethernet link down.
- DHCP failure.
- MQTT broker unreachable.
- NTP failure.
- RTC failure.
- I2C processor missing.
- I2C NAK/timeout.
- Wiegand processor IRQ storm.
- RS485 timeout.
- RS485 CRC error.
- Invalid MQTT topic.
- Invalid payload.
- Unknown Wiegand format.
- Invalid interface number.
- LCD missing.
- Extension board missing.

Important behavior:

```text
If NTP fails, continue using RTC.
If MQTT fails, continue local operation and reconnect.
If one interface fails, do not stop the whole system.
If extension board is missing, continue with interfaces 0 to 3.
If config is invalid, enter safe mode or restore backup.
```

---

## 30. Watchdog

Add watchdog support early.

The watchdog should only be fed when core services are healthy enough:

```text
main loop alive
MQTT service not locked
I2C manager not locked
RS485 manager not locked
```

After watchdog reset, publish online status with reboot reason if possible.

---

## 31. Logging

Recommended SD logs:

```text
/logs/system.log
/logs/transactions.log
```

Log important events:

- Boot started.
- Config loaded.
- MAC and serial number.
- Ethernet mode.
- IP address.
- MQTT connect/disconnect.
- MQTT discovery result.
- NTP update success/failure.
- RTC read/update.
- I2C scan results.
- Wiegand IRQ events.
- RS485 retries.
- Wiegand output result.
- Transaction publish result.
- Errors/warnings.

Use rotating logs later if possible.

---

## 32. Security Requirements

Minimum:

- MQTT username/password support.
- Only accept config update commands from authorized monitor serial number.
- Block path traversal in SD file access.
- Validate uploaded config before replacing active config.

Future recommended:

- MQTT TLS.
- Signed config files.
- Signed firmware updates.
- Per-command permissions.
- Topic ACLs in Mosquitto.

---

## 33. Codex Implementation Rules

Codex must follow these rules:

```text
Do not write one giant main.cpp.
Create clean C++ classes.
Use non-blocking service methods where possible.
Do not hard-code pins, topics, IPs, MAC address, LCD lines, or Wiegand formats.
All configurable values must come from config.json.
Do not hard-code Wiegand 26/34/48 formats.
Do not bit-bang Wiegand on the W6300-Pico2.
Use the existing I2C protocol commands exactly.
Implement RS485 retries and timeout handling.
Publish a monitor transaction result for every MQTT access request.
Use UTC internally.
Use timezone only for display/local formatting.
Implement in milestones.
Do not attempt to implement the full project in one step.
```

---

## 34. Details Still Needed Before Final Implementation

The following items must still be supplied or confirmed.

### Hardware Pinout

Current confirmed and reserved values:

```text
SD card SPI SCK       GP10
SD card SPI MOSI      GP11
SD card SPI MISO      GP12
SD card SPI CS        GP13

I2C SDA               GP4
I2C SCL               GP5

DS2401 1-Wire         GP6

Startup gate button   GP22
  active-low
  internal pull-up enabled

W6300 QSPI SCK        GP17   board-reserved
W6300 QSPI IO0        GP18   board-reserved
W6300 QSPI IO1        GP19   board-reserved
W6300 QSPI IO2        GP20   board-reserved
W6300 QSPI IO3        GP21   board-reserved

IRQ0                  GP14   reserved
IRQ1                  GP15   reserved
IRQ2                  GP16   reserved
IRQ3                  GP2    reserved
IRQ4                  GP3    reserved
IRQ5                  GP7    reserved
IRQ6                  GP8    reserved
IRQ7                  GP9    reserved

SD copy button        GP26   reserved
  active-low
  intended for external pull-up to 3V3 and button to GND
  not implemented yet

Available for future allocation:
  GP27
  GP28

Board-managed / avoid for general allocation:
  GP23   SMPS mode pin
  GP24   VBUS sense
  GP25   default status LED from board definition
  GP29   VSYS monitor / ADC
```

Still to be confirmed:

```text
RS485 UART TX/RX pins
RS485 RTS/DE pin
LCD I2C address
RTC I2C address
RGB LED pins for SD-to-LittleFS import/status workflow
```

### RS485 Protocol

Need exact protocol from the `roc_em_rs` project:

```text
command format
address field
checksum/CRC method
ACK/NAK response
timeout expectation
baud rate
line ending
max packet length
retry-safe commands
```

### MQTT Commands

Need final command list, for example:

```text
stc_access_request
stc_access_response
stc_status_request
stc_status_response
stc_online_status_request
stc_online_status_response
stc_sdcard_request
stc_sdcard_response
stc_config_update
stc_config_validate
stc_reboot_request
stc_factory_reset
```

### Payload Fields

Need final access request and transaction payload schema:

```text
card_number
person_id
full_name
transaction_type
timestamp
wiegand_format
raw bitstring support
RS485 action fields
```

### Wiegand Formats

Need all final Wiegand formats:

```text
26-bit
34-bit
35-bit
37-bit
48-bit
custom formats
parity rules
facility/card masks
```

### Linked I2C/RS485 Behavior

Need to define:

```text
Should every access request use both I2C and RS485?
Should RS485 happen before or after Wiegand output?
What happens if I2C succeeds but RS485 fails?
What happens if RS485 succeeds but I2C fails?
Should monitor success require both to succeed?
```

### Offline Behavior

Need to define:

```text
Queue transactions when MQTT is down?
How many transactions to store?
Where on SD?
Retry interval?
Delete after successful publish?
```

### Production Behavior

Need to define:

```text
safe mode behavior
factory reset method
firmware update method
serial console commands
diagnostic MQTT commands
```

---

## 35. Recommended First Coding Milestones

Work one milestone at a time.

Do not ask Codex to build the complete system at once.

### Milestone 1

```text
Build W6300-EVB-Pico2 Pico SDK C++ project skeleton.
Serial debug output.
Status LED heartbeat.
No Ethernet yet.
No MQTT yet.
No SD yet.
Create App and LedManager classes.
```

### Milestone 2

```text
Mount SD card.
Read /config.json.
Parse config.
Validate required sections.
Print loaded values over serial.
Do not start Ethernet yet.
```

### Milestone 3

```text
Start Ethernet.
Support DHCP/static IP from config.json.
Print IP address.
Show IP on LCD if LCD is already implemented, otherwise serial only.
```

### Milestone 4

```text
MQTT connect.
Subscribe.
Publish test status.
Implement basic SPV1.0 topic parser.
```

### Milestone 5

```text
I2C scan.
Send JMP/VER commands to Wiegand processors 0 to 7.
Initialize LCD.
Read RTC.
```

### Milestone 6

```text
NTP sync.
Update RTC.
UTC timestamp support.
Timezone display support from config.json.
```

### Milestone 7

```text
Parse SPV1.0 access request topic.
Parse MQTT payload.
Build Wiegand bitstring from wiegand_formats.json.
Send WIE command over I2C.
Publish transaction result.
```

### Milestone 8

```text
Implement Wiegand IRQ manager.
Configure 8 IRQ GPIO inputs.
Queue events from GPIO ISR.
Read tagged messages from Wiegand processor buffer when IRQ is asserted.
Publish corresponding MQTT events.
```

### Milestone 9

```text
RS485 manager.
RTS/DE control.
Retries.
Timeout handling.
ACK/NAK/CRC handling.
Transaction result reporting.
```

### Milestone 10

```text
Full linked interface transaction:
MQTT -> interface -> I2C + RS485 -> monitor transaction.
```

### Milestone 11

```text
Remote SD card read/list/tail support over MQTT.
Chunk large responses.
Restrict allowed paths.
```

### Milestone 12

```text
Remote config upload.
Chunk upload.
Validate temp config.
Backup active config.
Replace config safely.
Optional reboot.
```

---

## 36. First Codex Prompt

Use this prompt to start coding.

```text
Read docs/CODEX_SPEC.md completely.

Implement only Milestone 1:

- W6300-EVB-Pico2 Pico SDK C++ project skeleton
- serial debug output
- status LED heartbeat
- no Ethernet yet
- no MQTT yet
- no SD yet

Use clean modular C++ classes.
Create App and LedManager.
Do not put everything in main.cpp.
Do not hard-code future config values except temporary defaults needed for Milestone 1.
Do not implement later milestones yet.
```

---

## 37. Development Rule

After each working milestone:

```text
Build.
Flash.
Test on hardware.
Commit.
Then ask Codex for the next milestone only.
```
