# RTL8720CF V2/V3 firmware audit and GREE Livo telemetry map

This report records the static-analysis results for the two archived
RTL8720CF images associated with firmware code `362001065279` and compares the
recovered appliance-UART behavior with the captured GREE Livo Gen3 traffic.

## Audited images

| Module firmware | Advertised appliance protocol | Size | SHA-256 |
|---|---|---:|---|
| `U-WB05RT13V1.21-4307516678339185323.bin` | `V2.0.0` | 767,556 | `66f2fa23b3aad06493d14f98ec294175a1a8e4ae79921981b1af8851c1136163` |
| `U-WB05RT13V1.53-17090889809932733253.bin` | `V3.0.0` | 787,332 | `919443cdaf26da7085c21d7d78763272a4dbb302472c0d11038a67e836d0bc2a` |

Immutable upstream Git object IDs are recorded in
[`rtl8720cf-firmware-sources.md`](rtl8720cf-firmware-sources.md). The audit
workflow verifies those Git object IDs before analysis.

## Container and address-space layout

Both OTA containers have three loadable regions relevant to this audit:

| Version | Region | File offset | Runtime address | Length |
|---|---|---:|---:|---:|
| v1.21 | SRAM/runtime data | `0x220` | `0x10000480` | `0x4B28` |
| v1.21 | XIP application code | `0x8140` | `0x9B000140` | `0x8A8F0` |
| v1.21 | XIP strings/static data | `0x94140` | `0x9B800140` | `0x274B0` |
| v1.53 | SRAM/runtime data | `0x220` | `0x10000480` | `0x6A48` |
| v1.53 | XIP application code | `0x8140` | `0x9B000140` | `0x8F588` |
| v1.53 | XIP strings/static data | `0x98140` | `0x9B800140` | `0x28220` |

The code, strings and property registry must be loaded into one Ghidra program.
Analyzing either XIP image by itself loses cross-region references and produces
an incomplete result.

## Property registry

The ordered property-name registry is in SRAM and points into the
`0x9B800140` string region. It contains 476 entries in both images:

- v1.21 table: `0x10000A40`
- v1.53 table: `0x10000AAC`

The v1.21 value store is a `uint16_t` array at `0x10015310`; property index
`n` is stored at byte offset `n * 2`.

Relevant v1.21 entries are:

| Index | Offset | Property |
|---:|---:|---|
| 51 | `0x066` | `ElcEn` |
| 52 | `0x068` | `EnvTem` |
| 70 | `0x08C` | `HumSen` |
| 71 | `0x08E` | `ElcP` |
| 72 | `0x090` | `ElcOnKwh` |
| 73 | `0x092` | `ElcAllKwhClr` |
| 74 | `0x094` | `ElcAllKwhH` |
| 75 | `0x096` | `ElcAllKwhL` |
| 77 | `0x09A` | `TemSen` |
| 197 | `0x18A` | `EnergyFlow` |
| 198 | `0x18C` | `OutEnvTem` |
| 199 | `0x18E` | `InEvaTem` |
| 200 | `0x190` | `CompressorTem` |
| 201 | `0x192` | `CompressorFqy` |

v1.53 retains the electrical property group but replaces entries 198 through
201 with `NoD`. Its command `0x35` parser and retained payload layout remain
compatible; this is a cloud/property-schema removal, not evidence that the
appliance bytes moved.

## Startup sequence required before telemetry reads

The telemetry pages cannot be evaluated independently from adapter startup.
Both RTL images implement the same ordered sequence:

1. send the command-`0x02` identity frame;
2. send a full-MAC command-`0x04` solicitation and accept command `0x44`;
3. after identification resolves, send the initialized **29-byte** command-
   `0x03` buffer four times while the startup counter advances from zero to
   four;
4. only then continue normal report scheduling.

The command-`0x03` startup buffer is the same envelope later used for selector
reads. It carries module state in byte 26 and date/time in bytes 15 through 20.
Before clock synchronization, an all-zero date/time is valid. The ESP replacement
uses module state `1`, matching the router/cloud-connected state recovered from
the firmware.

The previous probe instead replayed a 17-byte command-`0x03` frame captured from
another adapter generation. It never performed the RTL four-frame startup stage.
That mismatch could affect the length and capability content of subsequent
`0x32` synchronization reports, so the old one-byte `0x32 = 00` result is retained
as historical evidence but is not treated as the final result of a complete RTL
startup.

## Exact RTL8720CF report-request format

The audited long read-only request is a 29-byte command-`0x03` frame:

```text
7E 7E 1A 03 ... reserved-byte checksum
```

`LEN=0x1A`, so the total frame length is 29 bytes. Earlier 28-byte experiments
used `LEN=0x19` and omitted the reserved byte immediately before the checksum;
their selector-6/7 results are not valid tests of the RTL8720CF request path.

### Primary selector

The primary selector is full-frame byte 4, request payload byte 0:

| Bit/value | Expected response |
|---:|---:|
| `0x00` | `0x31` extended status |
| `0x01` | `0x33` combined/general report |
| `0x02` | `0x34` indoor report |
| `0x04` | `0x35` outdoor operating report |

### Secondary selector

The secondary selector is full-frame byte 14, request payload byte 10:

| Bit/value | Expected response |
|---:|---:|
| `0x01` | `0x42` opaque service page |
| `0x02` | `0x41` opaque service page |
| `0x04` | `0x40` optional electrical page |

Full-frame bytes `8..10` and `11..13` are two HH:MM:SS context triples.
Firmware functions `0x9B027C14` (V2) and `0x9B02D128` (V3) only fill a
triple with `17 3B 3B` (23:59:59) when the corresponding high context flag in
byte 4 is set. They are **not RSSI fields**. Neutral selector reads leave both
triples entirely zero:

```text
0x31  7E 7E 1A 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 1E
0x33  7E 7E 1A 03 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 1F
0x34  7E 7E 1A 03 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 20
0x35  7E 7E 1A 03 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 22
0x42  7E 7E 1A 03 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 01 00 1F
0x41  7E 7E 1A 03 00 00 00 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 01 00 20
0x40  7E 7E 1A 03 00 00 00 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 01 00 22
```

The previous `00 00 3B` partial triples were malformed context values. Their
large checksums (`0x94..0x98`) resulted from adding those two spurious seconds
bytes; they were not an OEM requirement.

## Receive families

The v1.21 parser recognizes commands:

```text
31 32 33 34 35 36 3C 40 41 42 44 45 46 4D 53
```

v1.53 also recognizes `0x52`, which belongs to a board-update state machine.
Only `0x31` is a climate-state response and may acknowledge the normal climate
poll/control lifecycle. The others are known diagnostic, configuration or
service pages and must not be counted as unknown traffic.

## Firmware-confirmed Livo field map

Payload indexes below exclude sync, length, command and checksum.

### Command `0x31`: extended status

| Payload | Property | Decode | Confidence/limit |
|---:|---|---|---|
| `0` | `HumSen` | raw | Property field; captured value is not proven `%RH` |
| `23`, bits `3:2` | `UDFanPort` | `(raw >> 2) & 3` | Port/enum, not RPM |
| `7`, bit `3` | `ElcAllKwhClr` | boolean | Clear/request flag; not accumulated energy |
| `35`, bit `7` | `ElcErg` | boolean | Firmware-confirmed electrical flag |
| `35`, bits `6:3` | `ElcGear` | `(raw >> 3) & 0x0F` | Raw electrical/load gear, not percent |
| `42` | `TemSen` | `raw - 40` | Indoor temperature |
| `44` | `TemsSenOut` | `raw - 40` | Outdoor ambient temperature |
| `45` | `Elc1Kwh` | raw | Name confirmed; scale/unit unresolved |

The same assignments are present in the V2 and V3 status-property decoders.
The audited appliance-UART parsers do **not** locally assign `ElcP`,
`ElcOnKwh`, `ElcAllKwhH`, or `ElcAllKwhL`. Those names exist in the generic
property registry, but registry presence is not evidence that this appliance
UART supplies them. The opaque `0x40` handler retains its frame and updates
`ElcEn`; it does not decode those four values into the local property array.

The legacy Sinclair `(raw - 16) / 2` room-temperature formula is not correct
for the GREE four-speed layout. GREE uses the same `raw - 40` whole-degree
encoding as the OEM diagnostic reports.

### Command `0x34`: indoor report

The v1.21 decoder is at `0x9B0256D8`.

| Payload | Property | Decode | Confidence/limit |
|---:|---|---|---|
| `6` | `DFPoint` | packed raw enum | Firmware-confirmed extraction |
| `7` | target temperature | `raw - 40` | Capture-confirmed |
| `8` | `EnvTem` | `raw - 40` | Firmware-confirmed |
| `10` | `InEvaTem` | `raw - 40` | Firmware-confirmed indoor evaporator |
| `11`, bits `4:1` | `CpsTem` | `(raw >> 1) & 0x0F` | Raw property value |
| `25` | none | raw only | Not `HumSen`; prior humidity label removed |

### Command `0x35`: outdoor operating report

The v1.21 decoder is at `0x9B0257E8`. It writes:

- full-frame byte `0x09` / payload `5` to property `CompressorFqy`;
- full-frame byte `0x11` / payload `13` to `OutEnvTem`;
- full-frame byte `0x13` / payload `15` to `CompressorTem`.

The remainder of the operating cluster aligns with an independently
reverse-engineered outdoor-controller `0x31` page documented in
[`mkaluza/gree-hacking`](https://github.com/mkaluza/gree-hacking/blob/87965e596c15a509d62d5987cdc446c85accccfd/data_frame/packet.md).
The Wi-Fi report places that outdoor page's data byte 0 at payload index 3:

| `0x35` payload | Outdoor page data | Meaning | Decode |
|---:|---:|---|---|
| `4` | `1` | overall outdoor operating state | raw |
| `5` | `2` | compressor frequency | direct hertz |
| `6` | `3` | outdoor fan speed candidate | raw; RTL parser does not name it |
| `9` | `6` | expansion-valve closing candidate | raw/nonzero; RTL parser does not name it |
| `10` | `7` | electronic expansion-valve setting candidate | raw; step scale unverified |
| `13` | `10` | outdoor ambient temperature | `raw - 40` |
| `14` | `11` | outdoor coil/outlet temperature | `raw - 40` |
| `15` | `12` | compressor/discharge temperature | `raw - 40` |

This alignment makes `0x35[10] = 0xC8` a strong EEV-setting candidate rather
than volts, watts, current or compressor load. The RTL parser itself does not
name byte 10, so the component preserves the raw value and candidate label.

The captured Livo reports had `0x35[5] = 0` during thermodynamically proven
cooling operation. That proves the old request/result combination did not
produce a useful frequency value, but it does not settle UART availability:
the previous command-`0x03` builder was placing malformed partial time-context
triples in the request. The corrected neutral request must be tested before the
field is declared unpopulated by this model.

### Commands `0x41` and `0x42`

Both images retain these pages verbatim but do not decode them into the local
property array and do not feed them into the compressor/electrical properties.
They are now requested with the correct 29-byte frames and exposed raw for
comparison, but no physical units are assigned.

### Command `0x53`: separate `EnergyFlow` request family

This page is **not** reached through a command-`0x03` selector. Both firmware
versions contain a dedicated builder:

- V2 `0x9B02E438`
- V3 `0x9B0338F4`

The builder zeroes its transmit buffer and emits:

```text
7E 7E 32 09 [48 zero payload bytes] 3B
```

`LEN=0x32`, command is `0x09`, total wire length is 53 bytes, and the additive
checksum is `0x3B`. Scheduler state `0x0D` sends this frame and expects command
`0x53`. The receive dispatchers retain the full page and assign full-frame byte
`0x1C` / payload byte `24` directly to property index 197, `EnergyFlow`.

The scheduler checks retained `0x32` payload byte 1 bit 1 before enabling the
periodic request. The Livo's captured `0x32` payload contains only byte 0, so it
cannot advertise or deny this byte-1 capability. The research configuration
therefore sends one bounded command-`0x09` discovery request. If a real `0x53`
is received, later scheduled operating cycles may refresh it; a timeout or
fallback is not repeated merely because forcing remains configured.

The physical meaning and scale of `EnergyFlow` are unresolved. In the ordered
property registry it sits immediately after air-quality, gas, ventilation and
sensor-error properties and immediately before the temperature/frequency block;
it is **not** located in the earlier `Elc*` electrical-property group. It may
represent an airflow/energy-recovery state rather than electrical power. The
component therefore retains the complete `0x53` payload, publishes payload byte
24 as an unscaled OEM raw value, and
records whether the request received `0x53`, fell back to `0x31`, returned a
different page, or timed out.

## `ElcEn`, command `0x40`, and the short `0x32` page

There is no recovered appliance-UART transaction that writes or enables
`ElcEn`. The firmware keeps three signals separate:

- `0x32 payload[0] bit 0` advertises the optional command-`0x40` page;
- after a supported `0x40` arrives, `0x32 payload[1] bit 0` supplies the `ElcEn`
  property value;
- `0x32 payload[1] bit 1` gates the independent command-`0x09` / response-`0x53`
  `EnergyFlow` poll.

The earlier Livo capture `0x32 payload = 00` left the command-`0x40` page
unadvertised because payload byte 0 was present and its bit 0 was clear. The same
one-byte payload said nothing about either byte-1 bit. Because that capture was
made before the four-frame RTL startup sequence and with malformed time-context
bytes in later selector requests, the full-power research configuration now
performs one bounded corrected-frame `0x40` retry as well as the independent
command-`0x09` discovery. Normal configurations continue to honor the capability
bit.

Until the corrected hardware run, the accurate historical state is:

```text
0x40 electrical page: not advertised
0x53 EnergyFlow capability: capability byte missing
```

The firmware retains `0x40` as an opaque page. Although its property registry
contains `ElcP` and cumulative-kWh names, neither audited image maps a local
`0x40` byte range to voltage, current, watts or watt-hours. Those quantities
must remain raw until a real response is captured and correlated with an
external meter or the downstream cloud serializer is recovered.

## V2 to V3 behavioral changes

- The long UART report builder and selector layout remain compatible.
- The receive parser retains the core `0x31` through `0x53` families.
- V3 adds the `0x52` board-update path.
- V3 removes the named cloud-property assignments for `OutEnvTem`, `InEvaTem`,
  `CompressorTem` and `CompressorFqy`, replacing those registry entries with
  `NoD`.
- V3 still contains the explicit unsupported-`0x40` path and the electrical
  property family.
- The removed property names do not invalidate the stable `0x34`/`0x35` wire
  layout recovered from V2 and matched by the Livo captures.

## Data now exposed by the component

Normal semantic diagnostics:

- the V2 firmware field named `CompressorFqy` (reported with the existing hertz
  entity unit, but still awaiting a non-zero value from this Livo);
- outdoor fan speed candidate, raw;
- EEV position/setting candidate, raw;
- EEV closing candidate;
- outdoor operating state, raw;
- indoor, outdoor, evaporator, coil and compressor temperatures;
- electrical capability state;
- `ElcErg`, `ElcGear`, `Elc1Kwh` and `EnergyFlow` as raw/flag values.

Still intentionally raw:

- `0x40`, `0x41` and `0x42` payloads;
- phase current;
- DC-bus voltage;
- instantaneous watts;
- accumulated watt-hours/kWh scale;
- fan RPM conversion;
- compressor load percentage.

Assigning those missing units from a single plausible sample would be less
reliable than exposing the recovered operating fields now and preserving the
remaining pages for controlled correlation.

### Scheduled operating telemetry

The initial boot cycle performs the complete audited discovery sequence. Every
`repeat_interval`, the scheduler requests `0x35`. It also requests command
`0x53` only when the capability is advertised or a previous real `0x53`
response has already proven support. A forced unadvertised command-`0x09` is
limited to the first full-discovery cycle. The scheduler starts only when the
normal climate request lifecycle reports the UART idle.

Commands `0x36` and `0x3C` are retained as raw diagnostic/service payloads when
they appear. No synthetic `0x0C` service request is transmitted until its full
request semantics and safety preconditions are independently verified.
