# ESPHOME component to support Gree/Sinclair AC units
This repository adds support for ESP32-based WiFi modules to interface with Gree/Sinclair AC units.
This generally replaces stock WiFi module, sometimes giving a little more advanced features for swing control than stock application or remote.

**USE AT YOUR OWN RISK!**

Work is still in progress!

Tested with Sinclair AC (MV-H09BIF), mostly works, sometimes need to send parameter change twice - need to investigate.

Communication protocol is based on my own reverse-engineering.

ESPHome interface/binding based on:
* https://github.com/DomiStyle/esphome-panasonic-ac

**USAGE**
* Use at your own risk!
* See: https://github.com/piotrva/esphome_gree_ac/tree/main/examples
* Create configuration file: `ac-sinclair-main.yaml`
* Configure youe ESP `board`, `uart`, optionally `status_led`, check `wifi` settings (secrets)
* Create configuration(s) for your device(s): `ac-living-room.yaml`, `ac-bedroom.yaml`
* Configure deviceid and devicename, use proper `api` and `ota` keys
* Upload initial configuration to your ESP board using USB connection
* Disconnect completely power from your AC system, follow all safety procedures, desolder original WiFI unit
* Prepare a DIY adapter to connect ESP board to the AC unit, see table and representative schematic below
* Reconnect power to your AC system.
* Enjoy!

Generally stock WiFi module outputs UART with 3.3V signal levels and the AC unit outputs UART with 5V signal levels therefore a simple voltage divider on UART from AC unit towards ESP is usually suitable, considering very slow baudrate.
On some stock WiFi PCBs AC unit connector pins are marked on silkscreen.

| AC unit pin | Function | ESP connection        |
| ----------- | -------- | --------------------- |
| 1           | +5V      | VIN / 5V              |
| 2           | RX       | UART TX               |
| 3           | TX       | UART RX (via divider) |
| 4           | GND      | GND                   |

![Connection schematic](./images/schematic.png)

**NOTES**
* It was reported [#1](https://github.com/piotrva/esphome_gree_ac/issues/1) that with some changes the code works with Lennox li024ci AC

**TODO**
* Support Timers - maybe unnecessray as timers can be managed by Home Assistant
* Support Time sync - maybe unnecessray as timers can be managed by Home Assistant

## Receive-only compatibility and protocol discovery

The `sinclair_ac` component can safely observe candidate Gree Livo Gen3 and Gen4
units before it controls them. Set `transmit_enabled: false`; this is a software
TX lock and no UART write path (including polling or state packets) is used. Keep
the physical TX wire disconnected during initial work as an additional safeguard.
See [`examples/gree-livo-gen3-debug.yaml`](examples/gree-livo-gen3-debug.yaml).

Known `0x31` reports decode the existing Sinclair-compatible common prefix. Valid
frames with other command bytes are retained, counted, and optionally logged; they
never update climate state or cause a response. Additional bytes in a longer report
remain visible in raw packet logs, but are not assigned inferred meanings.

Before connecting any revision, verify connector orientation and logic/power
voltages. In particular, **never connect USB 5 V and HVAC 5 V simultaneously**.
After observing stable traffic and verifying pinout/voltage, explicitly enable TX
and reconnect it only when ready for controlled testing.

Useful captures include startup, power on/off, every operating and fan mode, each
vane position, display changes, sleep, X-Fan, save/8 °C heat, IR remote changes,
and fault states. Submit the labelled packet logs or serial captures with all Wi-Fi
credentials, API keys, MAC addresses, and location details removed. Climate action
is inferred from selected mode and room/target temperatures; it is not a compressor
running indication.

## Connector safety and staged compatibility process

This component is for the **blue Gree `WIFI` connector only**: it is a TTL UART (normally 4800 baud, 8E1) used by the factory Wi-Fi module. The red **`COM-MANUAL` connector is a separate RS-485 wired-controller bus**. The ports share neither electrical signaling nor packet format; do not connect this component to COM-MANUAL or treat it as compatible.

Use the following staged process on a live unit:

1. Meter and confirm connector orientation.
2. Confirm ground, supply, indoor RX, and indoor TX.
3. Confirm indoor-TX voltage and fit the required divider before connecting ESP RX.
4. Start with `protocol_mode: receive_only` and the physical TX wire disconnected.
5. Move to `poll_only` only after voltage verification.
6. Confirm repeated valid reports and stable checksums.
7. Move to `control` only after poll-only validation.
8. Power down before changing any wiring.
9. Never connect USB 5 V and HVAC 5 V simultaneously unless a verified power-selection circuit is present.

`receive_only` never calls a UART write method and ignores every Home Assistant control request. `poll_only` sends only the established no-change poll (never the `0xAF` apply marker) and ignores Home Assistant controls. `control` is the backward-compatible default. The old `transmit_enabled` option is deprecated (`false` maps to receive-only and `true` to control); it cannot be combined with `protocol_mode`.

### Mode and diagnostics configuration

`protocol_mode` belongs directly under the `platform: sinclair_ac` climate entry.
The diagnostic entities belong one level below its `diagnostics:` key. For example:

```yaml
climate:
  - platform: sinclair_ac
    name: Bedroom AC
    protocol_mode: receive_only
    diagnostics:
      communication:
        name: Bedroom AC communication
      receive_only:
        name: Bedroom AC receive-only active
      poll_only:
        name: Bedroom AC poll-only active
      protocol_mode:
        name: Bedroom AC protocol mode
      too_short_frames:
        name: Bedroom AC too-short frames
      frame_timeouts:
        name: Bedroom AC frame timeouts
      fan_speed_field_1_raw:
        name: Bedroom AC fan speed field 1 raw
      fan_speed_field_1_low_3_bits:
        name: Bedroom AC fan speed field 1 low 3 bits
      fan_speed_field_2_raw:
        name: Bedroom AC fan speed field 2 raw
      fan_quiet_raw:
        name: Bedroom AC fan quiet raw
      fan_turbo_raw:
        name: Bedroom AC fan turbo raw
      fan_decode_status:
        name: Bedroom AC fan decode status
```

If ESPHome reports any of these keys as invalid (especially suggesting
`protocol_state` for `diagnostics.protocol_mode`), it has loaded a pre-mode
revision of this external component. Update the external-component source to a
revision containing the mode support, or remove the cached external component
and run validation again. Do not move the keys to a different indentation level:
the layout above is the supported schema.

For Livo fan-field discovery, leave `protocol_mode: poll_only` enabled and use the
IR remote to change only the fan setting. Enable `debug.log_packet_differences`
to identify changes at payload bytes 18, 4, 16, and 6. The fan diagnostics retain
the raw byte 18 value, its low three bits, byte 4, quiet/turbo flags, and whether
the current decoder recognizes the combination. The component deliberately keeps
the established four-bit fan-speed mask until labelled captures demonstrate that
bit `0x08` has a different meaning on all affected units.

For local development the examples use `type: local` sources. Real installations should pin the revision being tested, for example `source: github://OWNER/esphome_gree_ac@BRANCH_OR_TAG`, rather than demonstrating an unpinned upstream `main`. When using a Git source while iterating on a branch, set a short `refresh` interval or clear ESPHome's external-component cache so that the schema and C++ implementation are updated together.

## Protocol capture guide

Label captures with startup, power, mode, setpoint, fan, horizontal and vertical vane positions, display, sleep, X-Fan, save/8 °C heat, IR-remote changes, and faults. Capture repeated transitions and retain raw frames. Climate action remains inferred from selected mode and temperatures; it is **not** compressor-run telemetry. Do not assign meanings to unknown bytes without repeatable evidence. COM-MANUAL remains a separate RS-485 research project.

## Livo fan profile, telemetry discovery, and local API

For a Livo four-speed unit, set `fan_profile: gree_4_speed`. It decodes the low
two bits of report payload byte 4 as Auto, Low, Medium, and High; byte 18 is
retained only as a diagnostic field because it is not a fan-speed field on this
profile. `fan_profile: auto` detects the observed Livo `byte 18 == 0x08`
signature and otherwise preserves the legacy mapping; `sinclair_extended` always
uses the legacy dual-field seven-speed mapping.
Quiet and Turbo remain independent overlay flags.

Set `telemetry_discovery.enabled: true` to retain the most recent valid payload
for commands `0x31`, `0x33`, `0x44`, `0x40`, and other valid unknown commands.
The optional text diagnostics expose raw payloads without assigning physical
meanings to unverified bytes. Discovery is deliberately observational: it does
not create guessed temperature, pressure, electrical, or compressor entities.
Capture repeated labelled transitions before adding a decoded sensor.

The Livo poll-only example enables encrypted ESPHome native API access and the
ESP-hosted authenticated web REST/SSE API. Add these values to your local
`secrets.yaml` (do not commit real credentials):

```yaml
gree_livo_api_key: "32-BYTE-BASE64-KEY"
gree_livo_web_username: "gree"
gree_livo_web_password: "LONG-UNIQUE-PASSWORD"
```

Keep the web API on an isolated IoT network; it must not be exposed directly to
the public internet. `local: true` embeds its assets so normal local operation
does not require an external frontend host.

Polling is one-request/one-response: a second poll is never written while a
valid response is outstanding. A missing response is timed out after one second
and then retried. Control updates preserve the latest valid `0x31` payload as a
baseline and advance the apply/clear sequence only after a valid report arrives.
