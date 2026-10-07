# XE71 / Vireo R32 COM-MANUAL protocol evidence

## Scope

This file separates evidence that belongs to the target Vireo R32 + XE71
controller family from older Gree wired-controller captures.  The distinction
is intentional: sharing an RS485 physical layer or the \`7E 7E\` frame family
does not prove that two controller generations use the same startup/session
transaction.

The deployed Vireo XIAO package is receive-only.  Do not enable an active
legacy probe merely because the bus is quiet.

## Source matrix

| Family / source | Physical / framing evidence | Application-layer evidence | Status for Vireo |
| --- | --- | --- | --- |
| GREE Vireo R32 service material | AP3 wired controller is on \`COM-MANUAL\`; AP6 optional gas sensor also shares \`COM-MANUAL\` | No public initialization bytes in the service material reviewed | Target hardware evidence |
| GREE XE71 owner material | CN1 is the 485 interface for the four-core indoor-unit cable; CN2/CN3 are for smart-zone control | The separate smart-zone side supports up to 16 communication node addresses; no public CN1 frame dump or session bytes found | Target controller evidence |
| GREE XE72 owner/service material | Same CN1 485 / four-core topology; same 4003800101 main harness family; smart-zone multi-controller wiring uses node addressing and a 2-bit DIP setting on the final controller | Target sibling topology evidence only; no public CN1 packet dump | Target sibling evidence |
| GREE compatibility material | XE71 / MC20700970 is the supported wired controller for current Vireo; XK41 is explicitly obsolete/replaced by XE71 and uses the same 4003800101 four-core harness; XE71 can replace XK19 only when the interconnecting cable is also replaced | XK41 is the closest documented hardware lineage, but no public XK41 packet capture was found; XK19 wire/session identity must not be assumed | Target compatibility evidence |
| \`maxim-smirnov/gree-wired-proto\` XK19 capture | 1200 baud, 8N1, \`7E 7E\`, message type \`0x11\`, trailing XOR | XK19 \`FF->00 / 0x15\`, signature \`0C 30 83\`; \`00->FF / 0x0E\`; \`FF->40 / 0x16\` | Reference only |
| \`bekmansurov/gree-hvac-protocol#8\` GKH/XK76 capture | Same broad 1200-8N1 \`7E 7E\` family | GKH/XK76 \`FF->00 / 0x22\`, signature \`09 30 83\`; accepted early at cold start; \`FF->40\` expands \`0x17 -> 0x29\` | Legacy experiment only |
| October 2026 Vireo field qualification | XIAO GPIO6 TX / GPIO7 RX / GPIO4 DE, 1200-8N1; weak TTL RX pull-up removes floating receiver artifacts | Four exact GKH/XK76 registration attempts produced zero post-release bytes and zero post-release edges | Negative target evidence |

## What the October field work proved

The original short byte groups seen around registration were not Vireo replies.
After moving the receive timestamp to the actual DE-release boundary, those
bytes were shown to exist while the transmitter still owned the wire.  The
Seeed TP8485E receiver output is disabled during transmit because DE and /RE
are tied together.  Enabling the weak ESP32-side RX pull-up then removed those
artifacts completely.

The final qualification run produced, on every one of four attempts:

- \`tx_rx_edges=0\`
- \`tx_residue=0\`
- \`turnaround_edges=0\`
- \`phase_gap_edges=0\`
- \`pending_after_release=0\`
- \`rx_edges=0\`
- \`unvalidated_rx=0\`
- \`valid_delta=0\`

The health journal remained at zero UART bytes and zero GPIO edges after the
experiment.  Therefore there is no remaining evidence that the Vireo R32
recognizes the borrowed GKH/XK76 \`09 30 83 / FF->00 / 0x22\` bootstrap.

## XK41 lineage finding

The strongest public controller-lineage evidence found in this pass is XK41,
not XK19. GREE's compatibility material marks XK41 as obsolete and replaced by
XE71, and both list the same 4003800101 four-core harness. The XK41 owner
material also describes a four-pin indoor-PCB controller connection and
shielded twisted-pair communication wiring.

That makes XK41 a high-value protocol-research ancestor for XE71-44/G, but it
does **not** provide application bytes by itself. No public XK41 frame dump,
baud declaration or session transcript was found in the material reviewed.
Accordingly, this repository records the lineage but does not add an XK41
decoder or transmitter profile without a real capture.

The same compatibility guide says XE71 may replace an installed XK19 only when
the interconnecting cable is replaced. That is useful negative evidence against
treating the existing XK19 packet capture as an XE71 wire/session oracle.

## Legacy startup direction evidence

The October 3 logic-analyzer screenshots recover the previously missing XK19
wake/poll sequence explicitly. All three transcribed frames satisfy the public
length rule and XOR to zero:

```text
FF 40 status (28 bytes, body 0x16)
7E 7E FF 40 11 16 0C 30 83 80 77 00 04 00 00 04 00 A8 00 80 00 00 00 00 00 00 23 FB

~800 ms idle pause annotated in the capture

00 FF poll-shaped frame (20 bytes, body 0x0E)
7E 7E 00 FF 11 0E 00 00 02 01 82 86 6E 52 00 80 00 00 20 7B

controller reply (27 bytes, body 0x15)
7E 7E FF 00 11 15 0C 30 83 01 13 7D 03 10 E0 E0 08 00 0A 00 00 00 00 00 00 00 3A
```

This is the concrete meaning of the earlier report that the controller would
not answer the ordinary poll unless another packet was sent first: the first
packet shown is the legacy `FF 40 / 0x16` status layout. The composite capture
then shows the `00 FF / 0x0E` frame after the annotated pause and the
`FF 00 / 0x15` controller answer immediately afterward.

The README examples in `maxim-smirnov/gree-wired-proto` use the same three
route/body-length layouts but different payload bytes. That is useful evidence
that these are state-bearing frames, not three immutable magic byte strings.
The stable evidence is their framing/order; individual payload bytes must not
be promoted to constants without a field-specific reason.

There is also an address-semantics correction. The original README labels bytes
2 and 3 as `Src` and `Dst` and tentatively maps `FF` to the indoor unit
and `00` to the wired controller. However, issue #1 includes an indoor-unit
cold start with **no wired controller attached** that still emits both
`FF 40` and `00 FF` frames. Therefore the two bytes cannot safely be treated
as literal physical source/destination addresses yet. This repository retains
the historical parser field names for compatibility, but user-facing
diagnostics and analysis treat `00->FF`, `FF->00`, and `FF->40` as
structural route-byte pairs.

The public wired-controller captures now establish a stronger comparison
baseline than packet shape alone:

- `maxim-smirnov/gree-wired-proto` identifies the legacy XK19-family roles as
  indoor unit `FF` and wired controller `00`, at 1200-8N1.
- In its issue #1, an XK76 controller remains idle without an indoor unit.
  Feeding the controller a known `FF->40` indoor-status frame clears its E6
  state, and a subsequent `00->FF` poll causes the controller to answer with
  its `FF->00` state frame.
- A separate no-controller indoor-unit capture in the same issue shows startup
  `FF->40` status broadcasts interleaved with exactly seven `00->FF` polls
  before those polls stop.
- The GKH/XK76 capture in `bekmansurov/gree-hvac-protocol#8` similarly shows
  indoor status/poll traffic first; after roughly 2-3 seconds the controller
  sends its `FF->00 / 0x22` state and the indoor `FF->40` status expands
  from the pre-registration layout to the registered layout.

Those sources describe an **indoor-unit-first** legacy session family. They do
not prove XE71 behavior. The target Vireo R32 is materially different in the
current field state: its receive line remained at zero physical edges for the
entire passive cold-start observation. Therefore there is no evidence that the
Vireo is emitting the legacy indoor-first announcement/poll sequence at all.

GREE patent CN110762604B, filed in the XE71 era, documents an explicit
indoor-unit/wired-controller pairing phase in a multi-split CAN architecture:
after unit address allocation the indoor unit sends a pairing request to its
wired controller. This is **conceptual evidence only** for an explicit pairing
state machine; it is not evidence that Vireo CN1 uses CAN, the same packet
format, or those exact startup bytes.

The working hypotheses should therefore remain separate:

1. XE71/Vireo CN1 has a controller-presence or pairing phase not represented by
   the XK19/GKH packet captures.
2. Vireo may keep CN1 electrically silent until an XE71-compatible controller
   initiates or satisfies that phase.
3. A genuine XE71 capture can still prove that the broader 1200-8N1/7E7E family
   survives on this product generation, but the current silent bus does not.

The trace analyzer now emits a descriptive `session.pattern` field. It can
recognize the known legacy indoor-first ordering and status expansion without
calling that ordering XE71 protocol. The exact XK19 screenshot sequence is
classified as `legacy_indoor_first_then_controller_reply`; a later expanded
status frame upgrades the structural classification to
`legacy_indoor_first_then_controller_registration`. This is intended for
comparing a future XE71 capture structurally before assigning field semantics.

Passive firmware diagnostics follow the same provenance rule. The legacy
controller codec still needs a reference state internally, but passive health
logs now print `unit=UNLEARNED` and the controller-state diagnostic labels
that state as `provenance=legacy_codec_reference`. The `09 30 83` reference
may appear as `LEGACY_REF` only when the deliberately enabled silent legacy
probe uses it, or as `LEARNED` only after the explicit legacy probe learns
three bytes from a compatible `FF->40` frame. Passive XE71/Vireo captures do
not learn or publish a target registration signature.

The Home Assistant display labels follow that boundary as well. Existing YAML
keys remain stable for compatibility, but entities derived from the captured
GKH/XK76 state machine are visibly prefixed `Legacy` (polls, controller
responses, registered status/setpoint, FF40 appendix/layout, and controller
state codec). This prevents a passive Vireo dashboard from presenting those
reference concepts as already-proven XE71 semantics.

The Legacy Controller State Codec is also fail-closed in passive Vireo mode.
Its internal encoder is intentionally initialized from the known GKH/XK76
reference payload, but those seeded values are not target telemetry. Until a
legacy experiment is explicitly enabled or a compatible legacy signature is
actually learned, the entity publishes only:

```text
provenance=legacy_codec_reference unit=UNLEARNED live_data=NO
```

It must not expose the seeded reference mode, power, secondary-control,
setpoint or counter values as though they came from the Vireo. An explicitly
enabled but not-yet-learned legacy experiment is labeled
`provenance=legacy_codec_staged`; a silent reference probe is labeled
`legacy_reference_probe`; only a learned compatible legacy bus is labeled
`learned_legacy_bus`.

### Fail-closed legacy session gating

A checksum-valid route shape is not enough to drive the legacy transmitter.
The active GKH/XK76 experiment now arms only from the exact proven
`0x11 / 00->FF / body 0x0E` poll role, and it learns a three-byte unit
signature only from the proven pre-registration `0x11 / FF->40 / body
0x16|0x17` status roles. A route-compatible frame with another message type
or body layout remains capture evidence, but cannot arm registration or teach
the transmit template.

The offline trace analyzer uses the same rule. Legacy layout labels include the
message type as part of their identity, and a checksum-valid `FF->00` frame
with an unproven type/layout is no longer counted as a legacy controller reply.
This keeps a future XE71 capture from being made to look legacy merely because
two structural route bytes happen to match.

## Current-generation Vireo R32 control provenance

GREE's current GMS GW integration documentation explicitly lists VIREO R32
among supported split systems and shows its first two HVAC lines configured as
`DCBAS (DCBAS)`. The same integration surface provides direct run/stop,
setpoint, mode, fan, louver, alarm and room-temperature control/monitoring for
the supported indoor units.

The October 6 ToolBox 2.1.23 installer was extracted directly. Its
`setuputility.exe` retains symbols/source paths and its static `line_types`
table identifies the actual Gree HVAC line entry as `Gree` / internal code
`GR`. `CG4`, `CG5`, and `DCABAS/DCAB` are separate line types and must
not be described as Gree indoor protocols.

More importantly, GREE's current GMS-GW installation manual exposes the bus
boundary. The normal mini-split topology is GMS-GW -> XK76CA -> indoor unit.
XK76CA contains an XK76 and two harnesses. The XK76 owner manual independently
states that its 4-core terminal connects to the air conditioner while its
2-core terminal connects to a centralized controller. The GMS-GW wiring
diagram connects L1/L2 A/B to the XK76 controller-side A/B network. Therefore
GMS-GW firmware primarily gives us the supervisory/centralized-controller side
of XK76; it is not evidence for bytes on the 4-core COM-MANUAL indoor link.

The primary target remains the Vireo R32 indoor-side XK76/XE71 COM-MANUAL
session. An authentic XK76CA indoor-side capture is now the highest-value
reference. XK19/GKH/XK76 framing remains comparison evidence until that newer
Vireo-side transaction is observed.

## Current conclusion

The electrical target remains the 4-core COM-MANUAL indoor link. Current
GMS-GW documentation proves VIREO R32 has a supported local integration path,
but the published path interposes XK76CA: GMS-GW talks to the XK76 2-core
centralized-controller side while XK76 talks to the indoor unit on its 4-core
terminal. The unresolved boundary is therefore the Vireo R32 indoor-side
XK76/XE71 startup/session behavior, not the GMS gateway line label. Public
XK19/GKH work remains useful only for framing comparison.  XE71/XE72 documentation also
shows separate smart-zone CN2/CN3 node-address and termination settings, but
XK19 documentation exposes the same smart-zone topology.  That topology is
therefore capture metadata, not evidence that XE71 CN1 uses a different
addressed/session protocol.  Future captures should still record controller
model, CN1/CN2/CN3 usage, node-address settings and DIP state so topology can be
ruled in or out without promoting it into CN1 semantics.

Do **not** promote any of these reference facts into XE71 semantics without a
real XE71/XE72 capture:

- \`09 30 83\` as the Vireo unit signature
- \`0C 30 83\` as the Vireo unit signature
- \`FF->00 / 0x15\` as XE71 controller state
- \`FF->00 / 0x22\` as XE71 controller state
- \`FF->40 / 0x29\` as the XE71 acceptance signal
- the GKH/XK76 accept-counter location
- the XK19 attention/session packet timing

## Current controller-first hypothesis

The Vireo field evidence now supports a second, independent protocol
hypothesis that does not require any additional GREE hardware. Two audited
RTL8720CF GREE Wi-Fi-module firmware generations use a controller-first
appliance-UART startup at 4800-8E1 with additive `7E 7E LEN CMD ... SUM`
framing. Both emit the same command-`0x02` identity frame, retry a
MAC-bearing command-`0x04` up to six times while waiting for command
`0x44`, then emit the same 29-byte neutral command-`0x03` startup frame four
times before normal status/control polling begins.

This family is separate from the legacy wired `1200-8N1 / type 0x11 / XOR`
codec. There is not yet evidence that the Vireo's four-wire COM-MANUAL bus
uses the RTL application layer, so the implementation is intentionally a
bounded probe rather than a promoted protocol decoder.

The dedicated `oem_rtl_probe` path:

- owns the live UART at 4800-8E1 only for the bounded experiment;
- cannot be enabled together with any legacy GKH/XK76 transmit option;
- sends no climate-state command;
- retains and logs every additive-checksum-valid response independently of the
  legacy XOR parser;
- stops after six maximum `0x04` attempts and four startup-sync frames.

A single valid response would immediately distinguish this controller-first
family from the long passive-zero state and give an authentic Vireo command/
payload to continue from.

## Explicit legacy-probe gate

The component retains the GKH/XK76 encoder because it is valid research
evidence and useful on that family.  It now takes two explicit settings to
permit that transmitter path:

\`\`\`yaml
gree_wired_rs485:
  active_probe: true
  legacy_gkh_xk76_probe: true
\`\`\`

\`active_probe: true\` without the provenance acknowledgement is rejected by
configuration validation.  \`silent_bootstrap_probe\` and
\`persistent_controller\` are also rejected unless the legacy GKH/XK76 profile
is explicitly acknowledged.

The normal Vireo package keeps all four transmit-affecting controls safe:

\`\`\`yaml
active_probe: false
legacy_gkh_xk76_probe: false
silent_bootstrap_probe: false
persistent_controller: false
\`\`\`

## Capture analyzer

\`tools/analyze_gree_wired_trace.py\` accepts ordinary text/log captures and
Saleae-style async-serial CSV byte exports.  It:

- reconstructs frames using the declared body length;
- verifies the XOR checksum;
- reports source/destination, type and body length;
- preserves timestamps when available;
- preserves explicit ESPHome TX/RX direction and leaves generic sniffer frames
  labeled \`observed\`;
- reports inter-frame timing;
- extracts the first three payload bytes as a comparison signature on
  \`FF->00\` / \`FF->40\`;
- labels known public layouts by their source family instead of calling them
  generic Gree or XE71 frames.

Example:

\`\`\`bash
python3 tools/analyze_gree_wired_trace.py xe71-cold-start.txt
python3 tools/analyze_gree_wired_trace.py xe71-cold-start.txt --exclude-tx
python3 tools/analyze_gree_wired_trace.py saleae-async-serial.csv --json
# JSON output includes a descriptive session.pattern comparison field.
\`\`\`

A new XE71 capture that produces an unknown route/body length is intentionally
reported as \`unknown\`; the analyzer must not coerce it into the nearest
legacy layout. ESPHome's own transmitted legacy-probe frames remain visible as
\`direction=tx\` for auditability but can be excluded with \`--exclude-tx\`;
they must never be mistaken for received target evidence.

## Passive discovery log summarizer

`tools/analyze_gree_wired_discovery.py` summarizes the ESPHome evidence
surface produced by the passive scanner and GPIO edge monitor.  It deliberately
keeps three different evidence classes separate:

- physical GPIO edge activity;
- UART bytes decoded under each candidate serial profile;
- checksum-valid legacy `7E 7E` frames.

This matters because a wrong baud or parity can still decode plausible bytes.
The tool will report a profile's byte count, but it will not call that profile
valid unless the legacy parser actually produced a complete checksum-valid
reference frame.  Likewise, raw edge cadence is emitted only as a timing hint,
not as protocol identification.

Examples:

```bash
python3 tools/analyze_gree_wired_discovery.py xe71-passive.log
python3 tools/analyze_gree_wired_discovery.py xe71-passive.log --json
```

Possible top-level conclusions are intentionally descriptive:

- `electrically_silent`
- `edge_activity_without_uart_decode`
- `uart_decode_candidates_without_legacy_validation`
- `legacy_frame_evidence_present`
- `capture_contains_tx_evidence`

A capture containing the historical GKH/XK76 TX path is explicitly marked
non-passive so it cannot be mixed into genuine XE71/XE72 evidence by accident.

## Vireo R32 indoor-board topology

For the target VIR12HP230V1R32AH family, the official parts material identifies
main board 300002064385. The Vireo R32 PCB print labels the board family
GRJ869-A V1 and shows a dedicated COM-MANUAL connector beside DOOR-C(DRY-C),
T-SENSOR and the display-side headers. In the board legend, COM-MANUAL is the
wired-controller interface.

The Vireo R32 wiring diagram places the optional AP3 wired controller and AP6
gas sensor on the COM-MANUAL path. This matches the field photo history: the
red four-position factory COM-MANUAL pigtail has an existing factory branch and
an unused parallel leg. A passive tap on that unused leg is therefore the
preferred non-invasive location.

Do not move the experiment to T-SENSOR, DISP, SWING, HEALTH/UVC or
DOOR-C(DRY-C) merely because COM-MANUAL is quiet; those are different
functions. A direct board-header tap is useful only as a continuity/control
experiment to prove the factory pigtail is straight-through, not because a
different application bus is expected there.

### Board variants

- VIR09/VIR12 R32 indoor units: main board 300002064385
- VIR18/VIR24 R32 indoor units: main board 300002064384
- PCB print family in the service manual: GRJ869-A V1
- Gas sensor part used across the listed Vireo R32 indoor units:
  34002406001601

## October 3 sustained-silence qualification

The fully passive post-cleanup capture stayed electrically inactive for more
than one minute with manual DE LOW and RX idle HIGH. The health journal remained
at zero UART bytes, zero valid frames, zero controller polls and, critically,
zero GPIO7 edges.

The GPIO edge observer sits below UART framing. If it sees no transition for an
entire cold-start window, changing baud or parity on the unchanged topology
cannot reveal a hidden stream because there is no waveform to decode.
Accordingly, the 12-profile UART matrix is now conditional rather than the next
automatic experiment.

The next topology checks, in order of information value, are:

1. powered-off continuity from the unused factory pigtail leg to the board's
   COM-MANUAL header, confirming all four conductors are straight-through;
2. a receive-only direct tap at the COM-MANUAL board header as a control against
   a defective or non-parallel pigtail;
3. passive observation with a genuine XE71/XE72 attached, which can establish
   whether this bus is controller-initiated and otherwise silent;
4. only if physical RX edges appear without trustworthy UART decode, run the
   12-profile cold-start serial matrix.

## Next capture that can advance the protocol

The highest-value evidence is a passive, cold-power-up capture with a genuine
XE71 or XE72 connected to a compatible R32 indoor unit.  Record from before
indoor-unit power is applied through at least the first minute, then repeat
with one labelled state change at a time.

Capture requirements:

1. Keep the ESP/logic-analyzer tap receive-only.
2. Record exact byte timing, not only decoded frame text.
3. Preserve frames with bad XOR or unknown lengths; they may indicate a
   different session layer rather than noise.
4. Record which device was connected (XE71 or XE72), indoor model, and whether
   the local IR/display remains active.
5. Record controller-side topology metadata before the run: exact XE71/XE72
   model, which communication connector is used, smart-zone node address if
   configured, and DIP-switch state. These are controls, not assumed CN1 fields.
6. For labelled changes, alter only one field per run: power, setpoint, mode,
   fan, then vane.
7. Run the resulting text/CSV through \`analyze_gree_wired_trace.py\` before
   assigning semantics.

If no XE71/XE72 is attached, a silent COM-MANUAL bus is now a valid field
observation, not a reason to transmit more guessed legacy packets.

### Raw edge-cadence evidence

The ESP32 GPIO7 any-edge ISR now keeps a cumulative short-gap cadence journal
independently of UART decoding. It records the number of inter-edge samples,
minimum gap, maximum gap, and most recent gap for gaps up to 20 ms. Longer gaps
are treated as inter-burst idle for cadence purposes, while still counting in
the raw edge total.

This is intentionally physical evidence, not automatic baud detection. If a
real XE71/XE72 produces traffic while the configured UART profile is wrong,
the health line can still expose approximate UART-scale timing. Useful
reference bit periods are approximately:

| Candidate baud | One bit |
| ---: | ---: |
| 1200 | 833 us |
| 2400 | 417 us |
| 4800 | 208 us |
| 9600 | 104 us |
| 19200 | 52 us |
| 38400 | 26 us |

Repeated multiples are normal because a UART waveform only changes level when
adjacent bits differ. A single anomalously tiny gap is not enough to select a
baud; correlate cadence with repeated bursts or a logic-analyzer capture.

### Passive serial-profile scan overlay

A rotating scan alone is insufficient for a protocol that may speak only at
cold power-up. The scanner therefore has a configurable
`passive_scan_start_profile`. That decoder is staged immediately when the
Gree component starts, before the first scan window begins. On ESP-IDF the
Gree component intentionally runs before the UART component so it can force
manual DE LOW; the boot profile therefore changes the UART configuration fields
without prematurely installing/reloading the UART driver. ESPHome's UART
BUS-priority setup then installs the driver with the staged profile. Later scan
rotations reload the already-initialized UART normally.

If physical RX edges exist but the UART still cannot decode a trustworthy
frame, use **one breaker-off run per profile** and set the boot profile through
the scan-overlay substitution:

```yaml
substitutions:
  gree_wired_scan_start_profile: "9600-8E1"

packages:
  base: !include packages/gree-vireo-xiao-rs485-listen-only.yaml
  scan: !include packages/gree-vireo-xiao-rs485-passive-profile-scan.yaml
```

The firmware logs the selected boot profile twice—once when passive scanning is
enabled and once when that decoder is staged before UART setup. The discovery
analyzer now retains both declarations and flags disagreement, so a capture
cannot quietly be filed under the wrong breaker-cycle profile.

Only after that physical-activity gate is met, repeat all 12 current hypotheses:
`1200/2400/4800/9600/19200/38400` with `8N1` and `8E1`. Keep each
capture from before indoor-unit power-up through at least the first minute.

After the runs, compare the complete set at once:

```bash
python3 tools/analyze_gree_wired_matrix.py captures/*.log
python3 tools/analyze_gree_wired_matrix.py captures/*.log --json
```

The matrix tool requires one self-declared boot profile per capture, reports
missing and duplicate hypotheses, flags any capture containing legacy TX
evidence, and requires a measurable observation span of at least 60 seconds
for every profile. Unknown or sub-60-second durations make the matrix
incomplete. Edge/UART/legacy-valid evidence remains separate. A complete
passive matrix means the acquisition procedure was covered correctly; it does
**not** by itself identify XE71 protocol.
The scanner may continue rotating after its initial window, but only the
selected boot profile can be treated as having observed the complete startup
interval.

For receive-only serial discovery, layer
\`packages/gree-vireo-xiao-rs485-passive-profile-scan.yaml\` on the normal
Vireo package. It cycles 1200/2400/4800/9600/19200/38400 baud with both
8N1 and 8E1 decode hypotheses. It cannot enable DE or any controller TX path.

The scanner logs raw-byte counts for every profile but deliberately does not
lock merely because a decoder produced several bytes. Wrong baud/parity often
creates plausible-looking garbage. Automatic lock requires a complete
checksum-valid legacy \`7E 7E\` frame; otherwise the scanner keeps cycling so
unknown XE71/XE72 framing is not coerced into a legacy profile. Compare the
per-profile byte counts with the independent GPIO edge-cadence evidence.

### Legacy experiment overlay

For reproducibility, the historical GKH/XK76 bootstrap is kept in a separate
overlay package:

`packages/gree-vireo-xiao-rs485-legacy-gkh-xk76-probe.yaml`

It must be layered on top of the normal Vireo package deliberately.  The
overlay exists to reproduce the now-negative experiment; it is not a suggested
XE71 discovery method and it does not enable persistent runtime control.

## External references

- GREE XE71 owner manual:
  https://www.greecomfort.com/assets/documents/controllers/xe71-wired-controller/xe71-owner-s-manual.pdf
- GREE system documentation / controller compatibility:
  https://www.greecomfort.com/system-documentation/
- GREE XK41 owner manual (documented predecessor/replacement lineage):
  https://www.greecomfort.com/assets/documents/controllers/xk41-wired-controller/xk41-owner-s-manual.pdf
- GREE Vireo R32 service manual:
  https://www.greecomfort.com/assets/our-products/vireo-r32/documents/vireo-r32-service-manual-a.pdf
- XK19 reverse-engineering reference:
  https://github.com/maxim-smirnov/gree-wired-proto
- GKH/XK76 wired-controller capture:
  https://github.com/bekmansurov/gree-hvac-protocol/issues/8
