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
| GREE XE71 owner material | CN1 is the 485 interface for the four-core indoor-unit cable; CN2/CN3 are for smart-zone control | XE71 can participate in an integrated system with up to 16 communication node addresses; no public CN1 frame dump or session bytes found | Target controller evidence |
| GREE XE72 owner/service material | Same CN1 485 / four-core topology; same 4003800101 main harness family; multi-controller systems use node addressing and a 2-bit DIP setting on the final controller | Strong sibling-generation topology evidence, but still no public CN1 packet dump | Target sibling evidence |
| GREE compatibility material | XE71 / MC20700970 is the supported wired controller for current Vireo; XE71 and XK19 use different harnesses when interchanged | Strong warning against assuming XK19 wire/session identity | Target compatibility evidence |
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

## Current conclusion

The electrical target remains COM-MANUAL RS485, and the public XK19/GKH work
remains useful for framing and comparison.  The unresolved boundary is the
XE71 application-layer startup/session behavior.  XE71/XE72 documentation also
shows that this controller generation was designed for addressed multi-node
systems, so future captures must preserve controller address/topology state
instead of assuming a single anonymous slave like the early XK19 notes.

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
- reports inter-frame timing;
- extracts the first three payload bytes as a comparison signature on
  \`FF->00\` / \`FF->40\`;
- labels known public layouts by their source family instead of calling them
  generic Gree or XE71 frames.

Example:

\`\`\`bash
python3 tools/analyze_gree_wired_trace.py xe71-cold-start.txt
python3 tools/analyze_gree_wired_trace.py saleae-async-serial.csv --json
\`\`\`

A new XE71 capture that produces an unknown route/body length is intentionally
reported as \`unknown\`; the analyzer must not coerce it into the nearest
legacy layout.

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
5. For labelled changes, alter only one field per run: power, setpoint, mode,
   fan, then vane.
6. Run the resulting text/CSV through \`analyze_gree_wired_trace.py\` before
   assigning semantics.

If no XE71/XE72 is attached, a silent COM-MANUAL bus is now a valid field
observation, not a reason to transmit more guessed legacy packets.

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
- GREE Vireo R32 service manual:
  https://www.greecomfort.com/assets/our-products/vireo-r32/documents/vireo-r32-service-manual-a.pdf
- XK19 reverse-engineering reference:
  https://github.com/maxim-smirnov/gree-wired-proto
- GKH/XK76 wired-controller capture:
  https://github.com/bekmansurov/gree-hvac-protocol/issues/8
