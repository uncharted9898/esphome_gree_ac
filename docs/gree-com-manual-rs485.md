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

The two data conductors are **not +5 V supply outputs**; they go only to the
Seeed board's RS485 **A/B** terminals. Physical connector pin numbers and wire
colors remain intentionally undocumented until their orientation is recorded
unambiguously on the target harness.

For first connection:

1. Power the XIAO from USB for the first capture.
2. Leave the field-verified +12 V conductor disconnected during that USB-powered
   first capture.
3. Connect the verified GND/common conductor to Seeed GND and the two data
   conductors to Seeed A/B.
4. Leave the Seeed 120-ohm termination switch OFF when attaching to the already
   populated COM-MANUAL bus.
5. Do not disturb the factory device already attached to the split
   COM-MANUAL harness.
6. After passive capture is proven, the field-verified +12 V supply may be
   evaluated for the Seeed expansion board's dedicated 12 V input; never route
   that conductor to the XIAO 5 V pin.

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

This keeps credentials local while all hardware, diagnostics, parser and future
COM-MANUAL decoder changes remain in the repository package.

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

These lengths are **references, not compatibility gates**. A checksum-valid
frame using a known route but a different body length is retained as
`known_route_variant`. Unknown routes and unexpected message types are also
retained and exposed. This is deliberate so an R32 Vireo extension of the
older controller protocol is visible rather than discarded.

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
