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

Exact generated requests, using the recovered neutral module state and RSSI
magnitude fields, are:

```text
0x31  7E 7E 1A 03 00 00 00 00 00 00 3B 00 00 3B 00 00 00 00 00 00 00 00 00 00 00 00 01 00 94
0x33  7E 7E 1A 03 01 00 00 00 00 00 3B 00 00 3B 00 00 00 00 00 00 00 00 00 00 00 00 01 00 95
0x34  7E 7E 1A 03 02 00 00 00 00 00 3B 00 00 3B 00 00 00 00 00 00 00 00 00 00 00 00 01 00 96
0x35  7E 7E 1A 03 04 00 00 00 00 00 3B 00 00 3B 00 00 00 00 00 00 00 00 00 00 00 00 01 00 98
0x42  7E 7E 1A 03 00 00 00 00 00 00 3B 00 00 3B 01 00 00 00 00 00 00 00 00 00 00 00 01 00 95
0x41  7E 7E 1A 03 00 00 00 00 00 00 3B 00 00 3B 02 00 00 00 00 00 00 00 00 00 00 00 01 00 96
0x40  7E 7E 1A 03 00 00 00 00 00 00 3B 00 00 3B 04 00 00 00 00 00 00 00 00 00 00 00 01 00 98
```

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
| `35`, bit `7` | `ElcErg` | boolean | Firmware-confirmed electrical flag |
| `35`, bits `6:3` | `ElcGear` | `(raw >> 3) & 0x0F` | Raw electrical/load gear, not percent |
| `42` | `TemSen` | `raw - 40` | Indoor temperature |
| `44` | `TemsSenOut` | `raw - 40` | Outdoor ambient temperature |
| `45` | `Elc1Kwh` | raw | Name confirmed; scale/unit unresolved |

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

The remainder of the operating cluster aligns exactly with the independently
reverse-engineered outdoor-controller `0x31` page documented in
[`mkaluza/gree-hacking`](https://github.com/mkaluza/gree-hacking/blob/87965e596c15a509d62d5987cdc446c85accccfd/data_frame/packet.md).
The Wi-Fi report places that outdoor page's data byte 0 at payload index 3:

| `0x35` payload | Outdoor page data | Meaning | Decode |
|---:|---:|---|---|
| `4` | `1` | overall outdoor operating state | raw |
| `5` | `2` | compressor frequency | direct hertz |
| `6` | `3` | outdoor fan speed field | raw; RPM scale unknown |
| `9` | `6` | expansion-valve closing flag | nonzero = asserted |
| `10` | `7` | electronic expansion-valve setting | raw position; step scale unverified |
| `13` | `10` | outdoor ambient temperature | `raw - 40` |
| `14` | `11` | outdoor coil/outlet temperature | `raw - 40` |
| `15` | `12` | compressor/discharge temperature | `raw - 40` |

This alignment explains the previously unresolved `0x35[10] = 0xC8`: it is an
EEV setting of 200, not volts, watts, current, compressor load or a generic
fixed model scalar.

The captured Livo reports had `0x35[5] = 0`. That is a valid zero-frequency
sample; a fresh capture while the compressor is definitely running should show
a nonzero value if the indoor board populates the field on this model.

### Commands `0x41` and `0x42`

Both images retain these pages verbatim but do not decode them into the local
property array and do not feed them into the compressor/electrical properties.
They are now requested with the correct 29-byte frames and exposed raw for
comparison, but no physical units are assigned.

### Command `0x53`

`payload[24]` is copied directly to `EnergyFlow`. Its request source and
physical scale are unresolved, so it remains a raw diagnostic value.

## `ElcEn` and command `0x40`

There is no hidden appliance-UART transaction that writes or enables `ElcEn`.
The receive dispatcher keeps page support and the property value separate:

1. `0x32 payload[0] bit 0` advertises the optional `0x40` report page;
2. a received `0x40` must be long enough and `0x40 payload[44] bit 0` confirms
   that the page is supported;
3. only then is `0x32 payload[1] bit 0` copied into the `ElcEn` property.

The Livo capture `0x32 payload = 00` therefore explicitly reports that the
optional `0x40` page is not advertised. It is not a truncated capability
record. When no `0x32` has arrived at all, the probe permits one discovery
request; after an explicit zero capability it avoids repeated `0x40` probes.

The firmware retains `0x40` as an opaque page. Although its property registry
contains `ElcP` and cumulative-kWh names, neither audited image maps a local
`0x40` byte range to voltage, current, watts or watt-hours. Those quantities
must remain raw until a real `0x40` response is captured and correlated with an
external meter or a downstream cloud serializer is recovered.

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

- compressor frequency in hertz;
- outdoor fan speed field, raw;
- EEV position/setting as a raw value;
- EEV closing flag;
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

The initial boot cycle performs the complete audited discovery sequence. After
that, `repeat_interval` sends only the selector-4 request for command `0x35`.
This keeps compressor frequency, outdoor-fan raw value, valve state, EEV setting
and outdoor thermistors fresh without repeatedly querying the unrelated
combined, status and electrical pages. The scheduler starts only when the normal
climate request lifecycle reports the UART idle.

Commands `0x36` and `0x3C` are retained as raw diagnostic/service payloads when
they appear. No synthetic `0x0C` service request is transmitted until its full
request semantics and safety preconditions are independently verified.
