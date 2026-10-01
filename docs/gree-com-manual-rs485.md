# Gree COM-MANUAL / wired-controller RS485

This component is separate from the blue Wi-Fi-module TTL UART supported by
`sinclair_ac`. It targets the red `COM-MANUAL` wired-controller bus used on
multiple Gree indoor boards, including the GRJ869 family.

## Deployment target

The initial hardware target is the pre-soldered Seeed Studio XIAO ESP32-C3 +
RS485 expansion board:

| XIAO signal | GPIO | RS485 board function |
|---|---:|---|
| D4 | GPIO6 | UART TX |
| D5 | GPIO7 | UART RX |
| D2 | GPIO4 | DE + /RE direction control |

The deployment firmware in
`examples/gree-vireo-xiao-rs485-listen-only.yaml` is intentionally passive.
ESPHome's UART `flow_control_pin` provides the half-duplex direction signal,
and the component itself contains no UART write call. The Seeed board is
therefore left in receive mode during normal operation.

### Field-verified Vireo COM-MANUAL electrical roles

On the target R32 Vireo harness, field measurements confirmed four electrical
roles:

- approximately **+12 V DC** accessory supply
- **GND/common**
- the two remaining conductors are the **RS485 differential pair** and sit in
  the approximately 5 V signaling range relative to common

The two data conductors are **not +5 V supply outputs**. The target harness is
now field-qualified electrically as:

- pin 1 = +12 V accessory supply
- pin 2 = GND/common
- pin 3 -> Seeed **B**
- pin 4 -> Seeed **A**

The pin 3 -> B / pin 4 -> A orientation leaves the TP8485E receiver output
idle HIGH with DE LOW. The opposite orientation leaves receiver output LOW and
produces a one-byte startup/reconfiguration artifact, so it is not used.

For first connection:

1. During initial qualification the XIAO may be USB powered; the deployed
   target has also been field-tested from the Seeed board's dedicated +12 V
   input and pin-2 common.
2. Connect pin 1 only to the Seeed board's dedicated 12 V input, never to the
   XIAO 5 V pin.
3. Connect pin 2 to Seeed GND, pin 3 to Seeed B, and pin 4 to Seeed A.
4. Leave the Seeed 120-ohm termination switch OFF when attaching to the already
   populated COM-MANUAL bus.
5. Do not disturb the factory device already attached to the split
   COM-MANUAL harness.
6. Keep the deployment receive-only until target traffic is characterized.
   The Vireo R32 wiring diagram places both the optional wired controller and
   gas sensor on COM-MANUAL, so the gas-sensor path must remain undisturbed.

## Minimal per-device YAML

The full XIAO/RS485 definition lives in
`packages/gree-vireo-xiao-rs485-listen-only.yaml`. A device only needs its
identity, local secrets, and the package reference:

```yaml
substitutions:
  device_name: my-gree-vireo
  friendly_name: My Gree Vireo
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: !secret ota-pass

packages:
  gree_vireo_rs485:
    url: https://github.com/uncharted9898/esphome_gree_ac
    files:
      - packages/gree-vireo-xiao-rs485-listen-only.yaml
    ref: codex/add-compatibility-and-protocol-discovery-mode
    refresh: 5min
```

This keeps device-specific Wi-Fi/OTA values local while all hardware,
diagnostics, parser, Web UI and future COM-MANUAL decoder changes remain in
the repository package.

## Established framing profile

The current parser wraps the previously recovered wired-controller framing:

- 1200 baud
- 8 data bits, no parity, 1 stop bit
- frame begins `7E 7E`
- byte 2 = source
- byte 3 = destination
- byte 4 = message type; recovered traffic uses `0x11`
- byte 5 = body length
- body length includes the final XOR checksum byte
- XOR across the complete frame, including the trailing checksum, equals zero

Recovered traffic includes these route/layout references:

| Route | Reference body length |
|---|---:|
| `00 -> FF` | `0x0E` |
| `FF -> 00` | `0x15` |
| `FF -> 40` | `0x16` |

These lengths are **references, not compatibility gates**. Captured
Gree-derived startup/status traffic also establishes an `FF -> 40` body
length of `0x17`, which is accepted as a reference layout alongside
`0x16`. A checksum-valid frame using a known route but another body length is
retained as `known_route_variant`. Unknown routes and unexpected message
types are also retained and exposed. This is deliberate so an R32 Vireo
extension of the older controller protocol is visible rather than discarded.

Do not attach fixed semantic labels such as "wired controller" or "indoor unit"
to address `00` solely from the earliest captures. Later bench work showed a
wired controller can remain completely idle without an indoor unit, while a
Gree-derived indoor unit with no wired controller attached emitted a short
`00 -> FF` discovery burst at power-up and repeated `FF -> 40` status
traffic. The monitor therefore treats source/destination addresses
observationally until the target Vireo exchange proves their roles.

The deployment monitor publishes every valid frame, including duplicate
payloads, so Home Assistant timestamps represent actual bus freshness.

## Current scope

The initial component deliberately does not assign climate meanings to payload
bytes that have not been proven for the new unit. It provides:

- raw complete frame
- raw payload
- source and destination
- route classifier
- frame-layout classifier
- body length
- per-route counters
- checksum, invalid-length, and partial-frame counters
- changed payload-byte indices relative to the previous frame on the same route
- bus-active state
- an explicit listen-only state

After collecting labelled traffic from the Vireo (power, mode, temperature,
fan and vane changes), validated field decoders and controlled writes can be
added without replacing this transport/capture layer.
