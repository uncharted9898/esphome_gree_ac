# Gree Livo Gen3 power-data discovery

This component exposes the RTL V2 field named `CompressorFqy` and retains the
matching outdoor-controller fan and EEV candidate bytes. Only the frequency
property name is a direct RTL assignment; the fan and valve labels come from a
separate GREE outdoor-bus layout and remain provisional. The component still
does **not** assign volts, amps, watts, watt-hours or fan RPM to an unverified
scale.

## What the July 23 capture proves

The recovered outdoor report was:

```text
04 00 40 00 11 00 00 00 00 00 C8 00 00 3F 48 55 ...
```

With the established `raw - 40` temperature encoding:

- `0x35[13] = 0x3F` -> outdoor ambient candidate, 23 C
- `0x35[14] = 0x48` -> outdoor coil candidate, 32 C
- `0x35[15] = 0x55` -> compressor discharge candidate, 45 C
- `0x35[10] = 0xC8` -> EEV position/setting candidate, raw 200

The ambient field stayed nearly constant while coil and compressor values rose
under cooling load. The RTL8720CF parser directly assigns byte 5 to
`CompressorFqy`. The byte-10 EEV interpretation comes from alignment with a
separate GREE outdoor-controller page and remains a candidate until this exact
Livo changes it live.

## Two different energy-related request families

Command `0x40` is the optional extended/monthly electrical page selected through
command `0x03`. The earlier Livo capture's one-byte `0x32 = 00` left that page
unadvertised, but that capture preceded the corrected four-frame RTL startup
sequence and used malformed time-context bytes in the selector request. The
full-power build therefore performs exactly one corrected, forced `0x40` read;
normal builds still honor the capability bit.

The deeper V2/V3 audit recovered a second read path that the old selector sweep
never sent:

```text
7E 7E 32 09 [48 zero bytes] 3B  -> expected response 0x53
```

The firmware names payload byte 24 of response `0x53` `EnergyFlow`, but does not
establish a unit or scale. The full discovery configuration therefore:

1. sends one bounded forced command-`0x09` request because the Livo's short
   `0x32` omits the byte that carries this capability;
2. retains the complete `0x53` payload;
3. publishes payload byte 24 as `EnergyFlow Raw`;
4. reports `received_0x53`, `fallback_0x31`, another command, or `timeout`;
5. refreshes `0x53` periodically only if the appliance advertises it or a real
   response has already proven support.

The same audit corrected two other material problems:

- after a valid `0x44`, the RTL scheduler sends four complete 29-byte command-
  `0x03` startup frames; the previous component sent a 17-byte frame from a
  different adapter generation;
- selector-request bytes
`8..10` and `11..13` are time-context triples. The older probe wrote lone
`0x3B` values into their seconds positions and called them RSSI. Correct neutral
requests leave all six bytes zero, so prior zero-frequency captures do not close
the question of what a byte-exact OEM request returns.

## Controlled test sequence

Use an external true-power meter or clamp meter as the reference. Record the reference values and the ESPHome payload after each stable step.

1. **Unit off, electronics energized** — wait two query cycles.
2. **Fan-only** — use a fixed indoor fan speed and wait two cycles.
3. **Cooling startup** — set a large temperature difference and capture the first five minutes.
4. **Cooling steady state** — wait until compressor and outdoor fan settle.
5. **Change demand** — raise the setpoint near room temperature, then lower it again.
6. **Shutdown** — capture at least two cycles after the compressor stops.

A likely field should satisfy more than one test:

- **Current or watts:** near zero when off, rises immediately with compressor load, varies with demand.
- **DC-bus voltage:** remains in a high, comparatively narrow range while powered and does not follow load linearly.
- **Compressor frequency:** near zero when stopped, ramps after startup, often changes in discrete steps.
- **Accumulated energy:** monotonic, changes slowly, and survives short load transitions.
- **Fan RPM:** changes with fan command or thermal demand but may remain nonzero after compressor shutdown.

## Interpreting candidate integers

For a byte pair `AA BB`, test both conventions until the firmware parser is recovered conclusively:

```text
little-endian = AA + (BB << 8)
big-endian    = (AA << 8) + BB
```

Then test common decimal scales only when the raw trend matches a reference instrument:

```text
raw
raw / 10
raw / 100
raw * 10
```

Do not select a scale merely because one sample looks plausible.

## Current implementation status

- `0x35[5]` is exposed as the firmware-named `CompressorFqy` field; the target
  must still demonstrate a non-zero live value after the corrected request.
- `0x35[6]`, `[9]` and `[10]` expose outdoor-fan and valve/EEV candidates as raw values.
- `0x40` remains raw; the previous capture reported it unadvertised, and the
  full-power build now performs one corrected-frame retest.
- `0x53` is retained raw for the newly recovered `EnergyFlow` read path.
- `power_discovery_summary` reports the recovered operating fields and capability state.
- `0x34[6]` and bit 5 are exposed because the byte changed from `0x20` in an earlier capture to `0x00` during active cooling.
- No electrical Home Assistant device classes are assigned yet.

## Refresh cadence

The full discovery sequence runs once at startup. Every configured repeat
interval requests `0x35`; it also refreshes `0x53` only after advertised or
proven support. The probe waits for the climate request lifecycle to become idle
before taking the UART.
