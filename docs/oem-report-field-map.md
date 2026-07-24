# Gree Livo Gen3 OEM report field map

This note records the evidence used by `gree_oem_report_sensors`. Payload indexes
exclude the `7E 7E length command` envelope and checksum.

## Captured reports

The controlled Livo Gen3 capture produced these 45-byte payloads while cooling
with a 25 C target and a simultaneously reported 24 C room temperature:

```text
0x34 indoor
04 00 40 00 11 01 20 41 40 00 3C 01 00 00 00 00
00 00 00 00 00 00 00 00 00 44 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00

0x35 outdoor
04 00 40 00 11 00 00 00 00 00 C8 00 00 40 46 51
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00
```

The observed temperature-like values use `raw - 40`:

| Report | Payload byte | Raw | Decoded | Current interpretation |
|---|---:|---:|---:|---|
| `0x34` | 7 | `0x41` | 25 C | target temperature; confirmed |
| `0x34` | 8 | `0x40` | 24 C | current/room temperature; confirmed |
| `0x34` | 10 | `0x3C` | 20 C | indoor coil candidate |
| `0x34` | 25 | `0x44` | raw 68 | no named RTL8720CF property |
| `0x35` | 13 | `0x40` | 24 C | outdoor ambient candidate |
| `0x35` | 14 | `0x46` | 30 C | outdoor coil candidate |
| `0x35` | 15 | `0x51` | 41 C | compressor discharge candidate |

The RTL8720CF V2 parser confirms payload 5 as `CompressorFqy`, payload 13 as
`OutEnvTem`, and payload 15 as `CompressorTem`. The matching outdoor-controller page documented in
[`mkaluza/gree-hacking`](https://github.com/mkaluza/gree-hacking/blob/87965e596c15a509d62d5987cdc446c85accccfd/data_frame/packet.md)
independently aligns payload 6 with the outdoor-fan field, payload 9 with the
expansion-valve closing flag, and payload 10 with the EEV setting. The
captured `0xC8` therefore represents an EEV position/setting of 200 rather than
an electrical or generic operating value.

## Firmware-confirmed change regions

Static analysis of the public CS532AX/MT7687 firmware recovered the incoming
report parser. For `0x34`, its change detector compares:

- payload byte 14 bit 5;
- payload bytes 20 and 21;
- payload bytes 36, 37, and 38.

For `0x35`, its change detector compares:

- payload byte 17 bit 2;
- payload bytes 21 through 28;
- payload byte 30;
- payload byte 31 bit 6;
- payload byte 36.

These fields are therefore useful fault-report diagnostics, but the parser does
not establish active polarity or code meanings. The component exposes raw
values and bit states rather than inventing an `active_fault` sensor.

## OFF-state capture limitation

The later log cleanly verified the normal `0x31` power transition at payload
byte 4 (`0x91` to `0x11`). However, the retained `0x34` and `0x35` records in that
log predate the OFF command. They cannot be used as OFF-state samples.

A useful next capture should retain a fresh `0x34`/`0x35` query set at several
points after shutdown (for example 0, 1, 3, 5, and 10 minutes) and again through
compressor startup. That will verify nonzero compressor frequency, outdoor-fan
field behavior and EEV movement without changing any control bytes.

## Compatibility

The original byte-number configuration keys remain supported. New semantic
candidate keys publish the same decoded values so existing YAML continues to
work while new installations can use clearer entity names.
