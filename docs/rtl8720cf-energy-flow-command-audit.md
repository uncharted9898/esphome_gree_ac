# RTL8720CF operating and EnergyFlow telemetry audit

This audit reopens the conclusion that the Livo Wi-Fi UART had already been
exhaustively queried. The previous selector sweep covered every primary and
secondary value inside command `0x03`, but it did **not** cover every independent
appliance-UART command family implemented by the RTL8720CF firmware.

The evidence below comes from both archived module images:

- `U-WB05RT13V1.21-4307516678339185323.bin` — communication protocol V2
- `U-WB05RT13V1.53-17090889809932733253.bin` — communication protocol V3

The images are identified by the immutable Git blob IDs already recorded in
`docs/rtl8720cf-firmware-sources.md`.

## Corrected conclusion

There is one material, firmware-authentic read path that had not been sent by
the ESPHome component:

```text
request command 0x09 -> response command 0x53 -> property EnergyFlow
```

This is not another command-`0x03` selector. Testing primary selectors `0..7`,
secondary selectors `0..7`, and module-state values `0..7` could never exercise
it.

The new runtime implementation therefore adds a bounded command-`0x09` probe.
An unadvertised request can be forced only once during the initial full-discovery
cycle. Advertised or previously proven support may then be refreshed during the
scheduled operating cycle. The component retains the complete `0x53` payload
and publishes only the firmware-named `EnergyFlow` byte as an unscaled raw
value. The property name alone does not prove watts, compressor load, or
accumulated electrical energy.

## A second material correction: the old command-`0x03` context bytes

The selector request was 29 bytes long after the earlier reserved-byte fix, but
it was still not byte-equivalent to the RTL builders. Full-frame bytes `8..10`
and `11..13` were described as RSSI and emitted as partial triples
`00 00 3B`. V2 function `0x9B027C14` and V3 function `0x9B02D128` prove they
are HH:MM:SS context triples. The firmware only writes `17 3B 3B` when the
corresponding high context flag in full-frame byte 4 is set. A neutral selector
read leaves both triples completely zero.

The corrected neutral checksums are `0x1E`, `0x1F`, `0x20`, and `0x22`, not the
old `0x94..0x98`. Consequently, the earlier selector sweep and active-cooling
zero-frequency samples were generated with malformed context values. They
remain useful receive captures, but they are not sufficient evidence that an
exact RTL request cannot return richer values.

The request module-state byte also needed firmer attribution. State `1` is not
produced by the early Wi-Fi-mode mapper; it is assigned later when the OEM
`greecloud_login_tcp_parse` path logs a successful ACE-server connection. The
replacement deliberately uses state `1` to emulate the fully cloud-connected
adapter, and the research sweep still tests module-state values `0..7` with the
corrected neutral envelope.

## Exact command-`0x09` request

Both firmware versions build the same 53-byte frame:

```text
7E 7E 32 09
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
3B
```

Equivalent compact description:

| Full-frame bytes | Meaning |
|---:|---|
| `0..1` | synchronization `7E 7E` |
| `2` | declared length `0x32` |
| `3` | command `0x09` |
| `4..51` | 48 zero payload bytes |
| `52` | additive checksum `0x3B` |

Recovered builder functions:

| Version | Function |
|---|---:|
| V2 v1.21 | `0x9B02E438` |
| V3 v1.53 | `0x9B0338F4` |

Both builders clear their output buffer and assign `LEN=0x32` and `CMD=0x09`.
The component's `build_rtl_energy_flow_query()` reproduces that exact frame and
its native test rejects non-zero payload variants even when their checksum is
repaired.

## Capability advertisement is a distinct `0x32` bit

The command-`0x09` scheduler gate is independent from both known `0x40` bits:

| `0x32` field | Recovered purpose |
|---|---|
| payload `[0]` bit 0 | optional command-`0x40` page advertisement |
| payload `[1]` bit 0 | `ElcEn` property value after a supported `0x40` report |
| payload `[1]` bit 1 | command-`0x09` / `0x53` `EnergyFlow` poll advertisement |

The payload-index attribution is not based only on decompiler variable names.
The scheduler and receive dispatcher use the same retained-frame RAM buffer:

| Version | `0x32` receive-copy pointer literal | Scheduler pointer literal | RAM destination |
|---|---:|---:|---:|
| V2 | `0x9B027788` | `0x9B027078` | `0x10015B93` |
| V3 | `0x9B02CA08` | `0x9B02C090` | `0x10016CEB` |

The scheduler reads offset `+5` from that full-frame buffer. Since payload starts
at full-frame offset 4, offset `+5` is payload byte 1. Its Thumb bit test shifts
by 30, selecting bit 1. When the bit is set and the timer is due, both versions
select transmit state `0x0D`, build command `0x09`, and reload a `0x4E10`
(20,000 millisecond) interval.

The captured Livo synchronization payload is only one byte, `00`. It does not
contain payload byte 1 at all, so it can neither advertise nor deny the
`EnergyFlow` capability. The production-safe default reports `capability_byte_missing`; the research
configuration can issue exactly one forced read-only command-`0x09` request so
the truncated capability shape does not prevent the decisive hardware test. A
successful forced response becomes evidence that the page is usable on this
appliance; only then do scheduled operating cycles refresh it.

## Response `0x53` and the named property

Both receive dispatchers implement a real command-`0x53` case:

| Version | UART receive dispatcher |
|---|---:|
| V2 v1.21 | `0x9B0274DC` |
| V3 v1.53 | `0x9B02C794` |

The handler:

1. retains the complete response frame;
2. reads full-frame byte `0x1C`, which is payload byte `24`;
3. stores it at property-array offset `0x18A`;
4. marks the property set as changed.

The property registry resolves offset `0x18A` / index 197 to:

```text
EnergyFlow
```

That name exists in both V2 and V3. Its registry neighborhood is the
air-quality/ventilation block (`IDUAirQu`, `AirQu`, `ODUViti`, gas properties,
`PM2P5Sta`, `SorErr`, `CommErr`), not the separate `Elc*` electrical block. The
name may therefore describe an airflow or energy-recovery state. The firmware
does not supply a physical unit or scaling formula, so the component exposes an
OEM `EnergyFlowRaw` byte rather than calling it watts, percent compressor load,
amperes, or accumulated energy.

## What the `0x35` firmware actually proves

The direct V2 parser at `0x9B0257E8` assigns only these named operating values:

| `0x35` payload byte | V2 property |
|---:|---|
| `5` | `CompressorFqy` |
| `13` | `OutEnvTem` |
| `15` | `CompressorTem` |

V3 retains the report and its fault/status processing but replaces property
registry entries 198 through 201 with `NoD`. `EnergyFlow` at index 197 remains
named in V3.

The existing fan and valve interpretations are weaker evidence:

| `0x35` payload byte | Current interpretation | Evidence level |
|---:|---|---|
| `5` | compressor frequency | direct RTL V2 assignment |
| `6` | outdoor-fan value candidate | alignment with a separate GREE outdoor-controller page |
| `9` | valve-closing candidate | alignment with that outdoor-controller page |
| `10` | EEV position/setting candidate | alignment with that outdoor-controller page |
| `13` | outdoor ambient temperature | direct RTL V2 assignment plus live correlation |
| `14` | outdoor-coil temperature candidate | strong thermodynamic/live correlation, no named RTL assignment |
| `15` | compressor discharge temperature | direct RTL V2 assignment plus live correlation |

The code keeps the existing YAML keys for compatibility, but the research
examples and discovery summary now label bytes 6, 9, and 10 as candidates.

The regression suite also includes the real active-cooling Livo fixture:

```text
0x35[5]  = 0
0x35[6]  = 0
0x35[9]  = 0
0x35[10] = 200
```

That fixture prevents a synthetic non-zero test vector from being mistaken for
proof that this specific appliance populates those operating fields.

## Other command families reviewed

The broader V2/V3 transmit and receive audit does not support using the following
families as blind telemetry probes:

| Command/report | Recovered role | Telemetry conclusion |
|---|---|---|
| `0x36` | `CO2` and `CO2Level` property update | not compressor, fan, valve, or electrical-power telemetry |
| `0x3C` | smart-sleep profile structure | not a generic service page |
| `0x45`, `0x46` | configuration/provisioning block acknowledgements | not live operating data |
| `0x4D` | identifiers, region, and configuration fields | not live operating data |
| `0x52` | V3 update/block-transfer state | not live operating data |
| `0x40` | optional extended/monthly electrical report | target reports it unadvertised/unsupported |
| `0x41`, `0x42` | opaque optional pages | tested; target fell back to `0x31` |
| `0x53` | named `EnergyFlow` report | firmware-authentic path previously not transmitted |

The command-`0x05` and command-`0x06` builders copy persistent configuration
blocks and are controlled by provisioning counters. There is no evidence that
they enable `CompressorFqy`, fan, EEV, voltage, or current reporting. Replaying
them without the original persistent configuration would be a write-risk and is
not part of this change.

The transmit scheduler was also enumerated by state rather than inferred from
the selector request alone:

| Request | Recovered purpose | Response/path |
|---:|---|---|
| `0x01` | normal status/control exchange | `0x31` |
| `0x02` | module identity/startup | startup state |
| `0x03` | report selector family | `0x31`, `0x33`–`0x35`, optional `0x40`–`0x42` |
| `0x04` | full-MAC identification solicitation | `0x44` |
| `0x05`, `0x06` | persistent provisioning/configuration blocks | `0x45`, `0x46` |
| `0x08` | V3 block/update transfer | `0x52` |
| `0x09` | independent read-only `EnergyFlow` poll | `0x53` |
| `0x0C` | smart-sleep/configuration structure | related configuration state |
| `0x0D` | scheduler/configuration acknowledgement | no operating telemetry mapping found |

No distinct command-`0x0A` or command-`0x0B` frame builder was recovered in
either application image. They are therefore no longer counted as confirmed
request families merely because those byte values occur elsewhere in code.

## Electrical quantities that remain unresolved

The property registries contain names including:

```text
ElcEn
ElcP
ElcOnKwh
ElcAllKwhClr
ElcAllKwhH
ElcAllKwhL
ElcErg
ElcGear
Elc1Kwh
EnergyFlow
```

However, the audited RTL code does not establish trustworthy physical scaling
for volts, phase current, DC-bus voltage, watts, or accumulated kWh on this Livo.
No named property containing current/amperage, voltage/DC bus, RPM, torque, EEV,
or compressor-load terminology was found in either registry.

The direct status-property decoders assign only four members of the electrical
name family from the ordinary `0x31` image:

| Property | Source | Meaning limit |
|---|---|---|
| `ElcAllKwhClr` | payload `[7]` bit 3 | clear/request flag, not a reading |
| `ElcErg` | payload `[35]` bit 7 | raw electrical flag |
| `ElcGear` | payload `[35]` bits 6:3 | raw gear/level enum |
| `Elc1Kwh` | payload `[45]` | unscaled byte |

No appliance-UART decoder assignment was recovered for `ElcP`, `ElcOnKwh`,
`ElcAllKwhH`, or `ElcAllKwhL`. The `0x40` handler retains an opaque page and
updates support/enable state, but does not locally populate those values.

Accordingly:

- `CompressorFqy` may be presented as the firmware's raw frequency field; live
  confirmation is still needed because this Livo returns zero during cooling.
- `EnergyFlow` is exposed raw until a live `0x53` response is correlated with
  operating mode, airflow/ventilation state, compressor load, and an external
  power meter; electrical meaning is not assumed.
- fan and EEV fields remain explicitly marked as candidates.
- no volts, amps, watts, or kWh units are invented.

## Runtime safety policy

The implementation permits one forced, unadvertised command-`0x09` request
only during the initial full-discovery sequence. The full-power research
configuration also permits one corrected-frame `0x40` retry because the prior
capability result was gathered without the RTL four-frame post-`0x44` startup
stage. It does not blindly add either unproven page to every 30-second `0x35`
refresh cycle. A periodic `0x09` refresh
is enabled only after `0x32` advertises the capability or a real `0x53` response
has already demonstrated that this appliance supports the page.

Behavior is:

1. distinguish no `0x32`, a missing capability byte, an explicit clear bit, and an advertised page;
2. optionally allow one forced read-only discovery request when support is not yet proven;
3. record whether response `0x53`, fallback `0x31`, or a timeout occurred through
   generation-based request attribution, with another command classified
   separately rather than mislabeled as a timeout;
4. retain and publish the complete `0x53` payload;
5. decode only payload `[24]` as unscaled `EnergyFlowRaw`;
6. expose the result as `received_0x53`, `fallback_0x31`, `other_0xNN`, or
   `timeout`;
7. refresh command `0x09` during later scheduled operating cycles only if the
   page was advertised or the forced discovery request actually returned
   `0x53`;
8. restore normal climate ownership after every bounded response window.

The full-power research example enables both one-shot forced reads. Normal
configurations leave them disabled and send optional pages only when advertised
or previously proven.
That research example also repeats the complete `0..7` primary/secondary
selector matrix and the `0..7` module-state sweep using the corrected neutral
command-`0x03` envelope. The prior sweep used the malformed partial time
contexts and is not treated as dispositive.

## Audit tooling correction

The broad Ghidra audit previously omitted request command `0x09`, response
command `0x53`, and the 53-byte frame length from its command/signature sets.
That omission made the selector family appear more complete than it was.

The audit now recognizes:

- confirmed request commands `0x01..0x06`, `0x08`, `0x09`, `0x0C`, and `0x0D`;
- response commands `0x31..0x36`, `0x3C`, `0x40..0x46`, `0x4D`, `0x52`, and
  `0x53`;
- the 29-byte command-`0x03` signature;
- the 53-byte command-`0x09` EnergyFlow signature;
- the `0x53` to property-offset `0x18A` assignment candidate.

This closes the specific static-analysis gap that allowed command `0x09` to be
missed.
