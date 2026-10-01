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
`examples/gree-vireo-xiao-rs485-listen-only.yaml` began as a passive monitor
and now contains a tightly bounded startup-registration experiment. ESPHome's
UART `flow_control_pin` provides ESP-IDF-managed half-duplex direction. The
component remains in receive mode unless a checksum-valid target `00 -> FF`
startup poll arms registration; an ordinary ESP/OTA reboot against an already
running indoor unit therefore does not transmit.

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
- pin 3 -> Seeed **A**
- pin 4 -> Seeed **B**

This is the current field mapping for the target harness. It is deliberately
not inferred from the indoor unit's `FE` indication: the target flashes `FE`
three to four times during cold start whenever the Seeed A/B pair is connected,
regardless of A/B orientation. With the data pair disconnected, the startup
`FE` indication is absent.

For first connection:

1. During initial qualification the XIAO may be USB powered; the deployed
   target has also been field-tested from the Seeed board's dedicated +12 V
   input and pin-2 common.
2. Connect pin 1 only to the Seeed board's dedicated 12 V input, never to the
   XIAO 5 V pin.
3. Connect pin 2 to Seeed GND, pin 3 to Seeed A, and pin 4 to Seeed B.
4. Leave the Seeed 120-ohm termination switch OFF. Leave the 5 V selector at
   IN so the auxiliary 5 V terminal is not being sourced.
5. Do not disturb the factory device already attached to the split
   COM-MANUAL harness.
6. Keep the deployment receive-only until target traffic is characterized.
   The Vireo R32 wiring diagram places both the optional wired controller and
   refrigerant/gas-sensor path on COM-MANUAL, so that factory path must remain
   undisturbed.
7. Before another cold-start qualification run, add a 4.7k-10k pull-down from
   Seeed D2 / GPIO4 (the TP8485E DE + /RE direction control) to GND. This holds
   the RS485 driver disabled while the ESP32-C3 is in reset, before ESPHome can
   configure the UART flow-control pin.

## R32 startup FE finding and safety gate

The target Vireo has a repeatable startup observation: attaching the Seeed
RS485 A/B pair makes the indoor display flash `FE` several times and then
recover; swapping A/B does not remove the behavior, while disconnecting the
data pair does. The transient `FE` therefore cannot be used as evidence for
A/B polarity.

The important distinction is timing. Running firmware has repeatedly shown
`DE=0`, but that says nothing about the interval while the ESP32-C3 is in
reset and before ESPHome configures GPIO4. The Seeed expansion board ties
TP8485E DE and /RE to D2/GPIO4, so the next qualification gate is to hardware
bias D2/GPIO4 LOW through reset. A 4.7k-10k pull-down is strong enough to define
the reset state while remaining easy for the ESP32 to drive later.

Until a cold boot remains clean with that reset bias, do not add active
COM-MANUAL probes. If the startup `FE` still appears with DE hardware-biased
LOW, treat the direct Seeed connection itself as too intrusive for this shared
bus and move to a higher-impedance receive-only tap.

The monitor now also exposes electrical activity independently of the legacy
1200-8N1 frame parser: sampled RX transitions, RX-high percentage, decoded UART
bytes per health window, current DE state, and a latched `DE high seen`
diagnostic.

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
traffic.

For the bounded active experiment, those observations are now used as timing
evidence rather than as unconditional address semantics: registration is armed
by a checksum-valid `00 -> FF` startup poll from the target bus. The first
three payload bytes from target `FF -> 40` traffic are learned and substituted
into the outgoing controller state frame, replacing the previous hardcoded
`09 30 83` unit signature. This means OTA rebooting the ESP alone no longer
pretends to recreate the indoor unit's startup registration window.

The deployment monitor publishes every valid frame, including duplicate
payloads, so Home Assistant timestamps represent actual bus freshness.

## Controller runtime model

The COM-MANUAL bus is not the same transport as the Gree commercial
COM-BMS/CN3 Modbus interface. The latter uses 9600-baud Modbus RTU register
reads; the Vireo R32 wiring diagram places its optional wired controller on
COM-MANUAL, which is the 1200-baud `7E 7E` framed bus documented here.

Recovered wired-controller sessions show the indoor unit acting as the bus
master during controller discovery: the indoor unit emits `00 -> FF` polls,
the wired controller answers with an `FF -> 00` state frame, and successful
early registration expands the indoor unit's recurring `FF -> 40` status
broadcast. The controller frame contains a rolling accept counter; captures
show that the indoor unit requires that counter to differ between accepted
state changes.

The component therefore has a separate `persistent_controller` runtime mode.
It is disabled by default during Vireo qualification. Once registration is
field-proven, enabling it causes established `00 -> FF` runtime polls to queue
an `FF -> 00` controller-state response with the next accept counter. The
bootstrap-only field package keeps this false so protocol research cannot
silently turn into continuous control traffic.

## Structured controller state and registered telemetry

The recovered wired-controller traffic now has dedicated codecs rather than
being treated as opaque registration bytes.

### Proven controller-state layout

The `FF -> 00`, message-type `0x11`, body-length `0x22` frame is recognized
as the captured wired-controller state layout. The implementation preserves all
33 payload bytes and only gives semantic names to fields supported by current
captures:

| Payload offset | Meaning supported by captures |
|---:|---|
| 0..2 | target/unit signature copied from `FF -> 40` |
| 3 | mode + power raw byte; a working GKH controller report identifies `0x19` as Cool + ON |
| 4 | secondary control raw byte; semantics remain unresolved |
| 12 | setpoint × 2; for example `0x28` = 20.0 °C |
| 20 | accept/change counter; the indoor unit requires it to differ for an accepted state change |

Unknown bytes remain byte-for-byte preserved. The runtime state object is
therefore safe to extend as more labelled captures arrive without reconstructing
the frame from guessed defaults.

### Registered `FF -> 40` status

The exact `0x29` body length observed after successful wired-controller
registration is now a first-class layout. Registration acceptance requires this
specific registered form rather than the earlier loose rule of accepting any
body length larger than `0x17`.

The component separately publishes the registered appendix so changes can be
diffed without stripping the base status bytes manually. No temperature, fan,
coil or diagnostic meaning is assigned to appendix offsets until a labelled
capture proves it.

### Query/service traffic

The XK19 reverse-engineering thread demonstrates an additional transaction:
a controller can be made to answer after a preceding attention/session packet,
an approximately 800 ms delay, and then a poll. The exact bytes are currently
present only in logic-analyzer screenshots in that thread, not trustworthy
machine-readable captures. This repository therefore documents that query layer
but does **not** transmit a guessed service/query frame.

This distinction is intentional: normal controller state/control, continuous
`FF -> 40` telemetry, and service/query transactions are separate protocol
layers. Unknown service traffic remains disabled until exact bytes and timing
can be recovered or captured on the Vireo.

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
- sampled RX-level transitions and RX-high percentage
- coarse electrical-activity state independent of frame validity
- current DE state and a latched DE-high-seen diagnostic

After collecting labelled traffic from the Vireo (power, mode, temperature,
fan and vane changes), validated field decoders and controlled writes can be
added without replacing this transport/capture layer.
