# Active recovered OEM telemetry queries

> **Superseded implementation note.** The CS532AX analysis below was the first
> source of the report-selector hypothesis. The active component now uses the
> independently audited RTL8720CF V2/V3 29-byte request format described in
> [`rtl8720cf-telemetry-map.md`](rtl8720cf-telemetry-map.md). In particular, the
> old 28-byte `LEN=0x19` vectors omitted a reserved byte and must not be replayed.

The `gree_oem_boot_probe` component replays the captured startup exchange, then
transmits the read-only report requests recovered from the RTL8720CF V2/V3
firmware.

## Queries

The current probe sends the exact RTL8720CF command-`0x03` variants:

| Request | Expected response |
|---|---:|
| Combined/general fault | `0x33` |
| Indoor-unit fault | `0x34` |
| Outdoor-unit data/fault | `0x35` |
| Extended status | `0x31` |
| Secondary service pages | `0x42`, `0x41` |
| Optional electrical page | `0x40`, only when advertised or for one initial discovery attempt |

These frames do not contain climate-control state changes. They use the neutral
module-state and RSSI fields recovered from the RTL8720CF builder.

## Scheduling

At boot the climate component remains `receive_only` while the probe:

1. replays the captured `0x02`, `0x04`, and `0x03` startup sequence;
2. waits for the configurable `quiesce_delay`;
3. sends the recovered queries with at least a 900 ms response window;
4. drains the final response window;
5. restores normal `control` mode.

`quiesce_delay` defaults to 1800 ms so a normal 1500 ms poll-response lifetime cannot overlap a later periodic query cycle.

## Configuration

```yaml
gree_oem_boot_probe:
  uart_id: gree_uart
  climate_id: gree_ac
  frame_spacing: 450ms
  quiesce_delay: 1800ms
  query_recovered_data: true
  restore_control: true
```

The complete query cycle runs once at boot. A periodic cycle is opt-in and
refreshes only command `0x35`:

```yaml
  repeat_interval: 30s
```

Before a repeated `0x35` query, the component waits until the normal climate
request lifecycle is idle. Climate TX is paused only for the bounded report
window and then restored.

## Capturing results

Enable telemetry discovery and configure `gree_oem_report_sensors`. Commands
`0x32` through `0x53` that are recognized by the audited firmware are classified
as known diagnostic pages, retained, and exposed without acknowledging a normal
climate poll or control transaction.

Expected log descriptions include:

```text
combined/general report selector -> 0x33
indoor report selector -> 0x34
outdoor operating report selector -> 0x35
secondary electrical selector bit 2 -> 0x40
```

A unit may ignore a query it does not implement. A missing response is not automatically a parser or wiring failure; compare the checksum, timeout, and ordinary `0x31` polling diagnostics.
