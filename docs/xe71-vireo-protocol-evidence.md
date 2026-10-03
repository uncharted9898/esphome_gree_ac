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

## Current conclusion

The electrical target remains COM-MANUAL RS485, and the public XK19/GKH work
remains useful for framing and comparison.  The unresolved boundary is the
XE71 application-layer startup/session behavior.  XE71/XE72 documentation also
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

For a real XE71/XE72 cold-start qualification, use **one breaker-off run per
profile** and set the boot profile through the scan-overlay substitution:

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

Repeat for all 12 current hypotheses:
`1200/2400/4800/9600/19200/38400` with `8N1` and `8E1`. Keep each
capture from before indoor-unit power-up through at least the first minute.

After the runs, compare the complete set at once:

```bash
python3 tools/analyze_gree_wired_matrix.py captures/*.log
python3 tools/analyze_gree_wired_matrix.py captures/*.log --json
```

The matrix tool requires one self-declared boot profile per capture, reports
missing and duplicate hypotheses, flags any capture containing legacy TX
evidence, records observation duration, and keeps edge/UART/legacy-valid
evidence separate. A complete passive matrix means the acquisition procedure
was covered correctly; it does **not** by itself identify XE71 protocol.
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
