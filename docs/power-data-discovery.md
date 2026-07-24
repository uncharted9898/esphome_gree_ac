# Gree Livo Gen3 power-data discovery

This component does **not** assign volts, amps, watts, watt-hours, compressor frequency, or fan RPM to an unverified byte. The current evidence supports the three `0x35` temperature fields, but the electrical fields remain unresolved.

## What the July 23 capture proves

The recovered outdoor report was:

```text
04 00 40 00 11 00 00 00 00 00 C8 00 00 3F 48 55 ...
```

With the established `raw - 40` temperature encoding:

- `0x35[13] = 0x3F` -> outdoor ambient candidate, 23 C
- `0x35[14] = 0x48` -> outdoor coil candidate, 32 C
- `0x35[15] = 0x55` -> compressor discharge candidate, 45 C
- `0x35[10] = 0xC8` -> unresolved operating scalar, raw 200

The ambient field stayed nearly constant while coil and discharge values rose under cooling load. That behavior strongly supports the three thermistor assignments. It does not identify byte 10.

## Why command `0x40` matters

Recovered OEM firmware contains an energy-report path and sends a neutral command-`0x03` request intended to produce response command `0x40`. Some Livo Gen3 captures have not returned an identifiable `0x40` frame. The full discovery YAML therefore:

1. repeats the recovered query cycle every two minutes during testing;
2. retains the last `0x40` payload;
3. publishes a compact `power_discovery_summary`;
4. lists the first 20 bytes as raw bytes plus adjacent unsigned 16-bit little-endian and big-endian candidates;
5. keeps `0x35[10]` visible beside the energy response.

The summary is deliberately mechanical. It helps compare fields without asserting their units.

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

- `0x35[10]` remains `outdoor_operating_value_raw`.
- `0x40` remains a raw retained payload.
- `power_discovery_summary` exposes raw and 16-bit candidate interpretations.
- `0x34[6]` and bit 5 are exposed because the byte changed from `0x20` in an earlier capture to `0x00` during active cooling.
- No electrical Home Assistant device classes are assigned yet.
