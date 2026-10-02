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
and now contains a tightly bounded startup-registration experiment. The field
package drives the Seeed board's D2/GPIO4 DE+/RE input directly: LOW is receive
and HIGH is transmit. This avoids relying on UART RTS ownership for a board
whose receiver and driver enables are tied together.

Normal registration remains target-gated by checksum-valid startup/status
traffic. A separate silent-bus fallback is enabled on the Vireo field package:
after 5 seconds it may arm one bounded registration sequence only if the ESP
has received zero UART bytes and observed zero RX-level transitions. Any
electrical or decoded activity suppresses that fallback.

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
7. The October 1, 2026 breaker-off qualification used the firmware-owned
   GPIO4 direction path and reached the bounded registration experiment on a
   true HVAC/ESP cold start. Do not add another hardware-bias requirement merely
   to repeat that test; preserve the current field wiring while protocol
   qualification continues.

## R32 startup FE finding and safety gate

The target Vireo has a repeatable startup observation: attaching the Seeed
RS485 A/B pair makes the indoor display flash `FE` several times and then
recover; swapping A/B does not remove the behavior, while disconnecting the
data pair does. The transient `FE` therefore cannot be used as evidence for
A/B polarity.

A breaker-off cold-start qualification on October 1, 2026 exercised the same
manual GPIO4 direction path from a true HVAC/ESP power-up. After five seconds
of complete UART/electrical silence the firmware transmitted the four bounded
registration frames, each consuming about 333-334 ms at 1200-8N1, and returned
GPIO4 LOW after every flush. Short UART byte groups were received in the four
response windows, but none formed a checksum-valid recovered COM-MANUAL frame
and registration never became established.

That result closes the earlier "soft reboot missed the target startup window"
question. The next qualification target is receive-side timing and framing, not
another blind DE/polarity change. Response-window bytes are therefore treated
as unvalidated UART observations until they produce protocol-valid evidence.

The monitor now also exposes electrical activity independently of the legacy
1200-8N1 frame parser. GPIO7 has a passive any-edge interrupt counter in
addition to the older main-loop samples, so short bit transitions are not
mistaken for a permanently idle-high receiver. Each bounded registration
window records interrupt-timed RX edge count/first/last/span, UART bytes pending
at the first post-release FIFO probe, and first/last UART-drain offsets. These
observations remain diagnostic only; they are not treated as a registration
acknowledgement unless the recovered protocol parser supplies valid acceptance
evidence.

The late October 1 ESPHome 2026.9.1 field run exposed an instrumentation flaw
rather than proving those short byte groups were target replies or self-echo.
The four attempts produced 11 decoded bytes total: `F0 00 B4`, `FA 00 23`,
`F0 00`, and `E2 00 35`. Every group was already pending by the old software
probe and every old per-window edge counter was zero. However, the old manual-DE
path lowered GPIO4 and then emitted two log messages before it armed the edge
window and probed the UART FIFO. That created a multi-millisecond blind interval;
one observed attempt left roughly 13 ms between the DE-low log and the RX-window
log, which is longer than one 1200-8N1 character.

The corrected path now quarantines any UART bytes that are already pending
while manual DE is still HIGH, arms the GPIO7 edge window before releasing DE,
timestamps the DE release and FIFO probe before any logging, and separately
accounts RX edges observed while our own transmitter owns the wire. The next
field capture can therefore distinguish TX-era residue from a real immediate
Vireo response without changing baud, parity, A/B polarity, or registration
payload contents.

### October 2, 2026 TX-residue qualification

The corrected-turnaround build resolved the ambiguity. Across all four bounded
registration attempts, the component quarantined exactly three UART bytes
*before* DE was released and observed zero UART bytes after release:

- attempt 1: `D3 00 23`
- attempt 2: `F3 00 23`
- attempt 3: `E2 00 34`
- attempt 4: `FA 01 04`

Every attempt reported `pending_after_release=0`,
`unvalidated_rx=0`, and `valid_delta=0`. The FIFO probe occurred only
5-6 us after the measured DE release. The one edge attributed to each old
response window occurred 19-20 us after arming, while DE release occurred
26-27 us after arming, so those four edges were also pre-release turnaround
activity rather than target response traffic.

This means the earlier 2-4 byte groups must not be described as Vireo replies.
They are TX-era receiver-side artifacts. The Seeed XIAO RS485 V1.1 schematic
uses a TP8485E transceiver with DE and active-low /RE controlled together and
shows its optional receiver-output pull-up as not populated. The TP8485E
receiver output is high-impedance when /RE is high, so the XIAO GPIO7 UART input
can float while this board is transmitting. The Vireo package therefore enables
a weak ESP32-side `rx_idle_pullup`; this changes only the TTL receiver input
and does not bias the A/B bus.

The edge journal is also split at the DE-release boundary. `rx_edges` and
its first/last/gap timing now describe only post-release response activity;
`turnaround_edges` records the already-armed edges seen before response
ownership begins, and health-level electrical activity subtracts both TX-era
and turnaround edges. A subsequent run should therefore remain electrically
inactive unless the target itself changes the receiver line after release.

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

The current Vireo R32 product submittal identifies XE71 as an optional wired
controller. That makes XE71 behavior the target reference for this unit; older
XK19 and working GKH captures remain useful protocol evidence but are not
treated as proof that the Vireo R32 accepts the same bootstrap transaction.

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

## Semantic frame roles and staged controller actions

The parser assigns semantic roles only to layouts that have direct capture
evidence. This is more specific than the generic route/layout classifier:

- `00 -> FF / 0x0E`: controller discovery/runtime poll
- `FF -> 00 / 0x15`: legacy/alternate controller-state layout
- `FF -> 00 / 0x22`: captured controller-state/control layout
- `FF -> 40 / 0x16` and `0x17`: base indoor status layouts
- `FF -> 40 / 0x29`: registered/expanded indoor status layout

The Vireo package exposes the latest semantic role as well as dedicated
`00 -> FF` poll payload and byte-change diagnostics. This keeps the cold-start
capture readable without assigning climate meaning to bytes that have not yet
been proven on Vireo hardware.

Two ESPHome automation actions are available for fields already supported by
captures:

- `gree_wired_rs485.set_controller_setpoint` stages the controller
  setpoint using the captured degrees-Celsius-times-two encoding.
- `gree_wired_rs485.set_controller_mode_power_raw` stages the raw
  mode/power byte. It deliberately remains raw because a complete cross-model
  mode/fan mapping has not yet been established for this Vireo.

These actions **do not transmit**. They mutate the persistent controller-state
object only. A staged value can reach the bus only through the normal
poll-driven controller response path, which additionally requires successful
registration and `persistent_controller: true`. The qualification package
keeps `persistent_controller: false`, so adding these actions does not change
the traffic generated by the currently deployed Vireo test build.

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


## Long-pass controller and telemetry surface

The structured wired-controller state now exposes the fields with direct
cross-capture support while retaining every unresolved byte unchanged.

- The observed controller mode/power byte has a proven ON/OFF delta of
  `0x08` in the working GKH ESPHome capture (`0x11 -> 0x19` when powered
  ON in Cool). The component therefore provides a staged power action that
  toggles only that bit and preserves the rest of the byte.
- The captured setpoint remains body payload offset 12 using degrees-Celsius
  times two. The expanded `FF -> 40 / 0x29` appendix also contains a
  `0x28` byte at appendix offset 5 in the 20.0 C reference capture. Because
  the public reverse-engineering thread still treats that echo as a candidate,
  ESPHome publishes it as a diagnostic candidate rather than silently using
  it as authoritative climate state.
- A raw controller-payload-byte staging action exists for controlled
  reverse-engineering. It mutates the in-memory 33-byte controller image only;
  it cannot transmit directly and the qualification package still has
  `persistent_controller: false`.
- `ff40_indexed` publishes every decoded status payload byte as
  `index:hex`. This is intentionally redundant with the raw payload: it makes
  field correlation against room temperature, coil temperature, fan state,
  compressor state and other labelled changes practical without assigning
  guessed semantics.

The Wi-Fi-module `0x2C/0x2F` protocol documented by
`bekmansurov/gree-hvac-protocol` is not reused on COM-MANUAL. Its issue #8,
however, is direct wired-controller evidence and is used alongside
`maxim-smirnov/gree-wired-proto` for the `00 -> FF`, `FF -> 00`, and
`FF -> 40` state machine.

The XK19 issue also demonstrates a separate attention/session transaction
before a poll, with an approximately 800 ms delay. The exact bytes are only
available in logic-analyzer screenshots at present. No service-query packet is
transmitted until those bytes are recovered exactly or independently captured;
the implementation keeps service/query traffic separate from the proven
controller-state path.


## Retained cold-start frame journal

The first 16 checksum-valid COM-MANUAL frames after ESP boot are retained with
their semantic role, ESP uptime and complete raw frame. Five seconds after the
first valid frame starts the bounded capture window, the journal is replayed
once through the logger at the next health interval. Continuous `FF -> 40`
traffic cannot keep the window open indefinitely.

This complements the original 128-byte startup raw buffer: malformed or
not-yet-decodable traffic still appears in the raw capture, while complete
valid discovery, registration and expanded-status frames survive long enough
to be visible after Wi-Fi/API logging attaches.


### October 1, 2026 retained-edge qualification

The first field run with the GPIO7 any-edge ISR produced a useful distinction:
the old main-loop sampler remained at zero transitions while the interrupt
counter reached 277. The UART decoder captured only 12 bytes during the bounded
startup exchange and then remained completely quiet for the rest of the long
observation window. This proves the physical bus had short startup activity
that main-loop sampling could not see, while also showing that the activity was
bounded rather than continuous background chatter.

Because API/log attachment can occur after the four registration attempts
finish, the component retains every registration-window summary and replays the
complete set after 30 seconds. The corrected trace separates bytes quarantined
while manual DE is still HIGH from bytes observed after release, records
arm-to-DE and DE-to-FIFO-probe latency, first/last drain timing, post-release
interrupt edge count, first/last edge timing, edge span, minimum/maximum
inter-edge gaps, and the RX-edge count accumulated during our own TX. Those
measurements are evidence for the actual signaling cadence and should be used
before changing baud/parity or controller frame contents.
