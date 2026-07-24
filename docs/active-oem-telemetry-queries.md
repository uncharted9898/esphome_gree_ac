# Active recovered OEM telemetry queries

The `gree_oem_boot_probe` component can now transmit the read-only request frames recovered from the CS532AX/MT7687 OEM firmware after replaying the captured startup sequence.

## Queries

The probe sends four command-`0x03` request variants:

| Request | Expected response |
|---|---:|
| Combined/general fault | `0x33` |
| Indoor-unit fault | `0x34` |
| Outdoor-unit data/fault | `0x35` |
| Extended/monthly energy | `0x40` |

These frames do not contain climate-control state changes. They use the neutral module-state fields recovered from the OEM builder.

## Scheduling

At boot the climate component remains `receive_only` while the probe:

1. replays the captured `0x02`, `0x04`, and `0x03` startup sequence;
2. waits for the configurable `quiesce_delay`;
3. sends the four recovered queries 450 ms apart;
4. waits 500 ms after the final request;
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

The query cycle runs once at boot by default. A periodic cycle is opt-in:

```yaml
  repeat_interval: 15min
```

During a repeated cycle normal climate TX is paused, the queries are sent, and control is restored. Climate commands submitted during that brief receive-only interval are ignored by the existing climate component, so long intervals are preferred while research is in progress.

## Capturing results

Enable telemetry discovery with a history depth of 64 and expose `capture_export_csv`. Responses `0x34` and `0x35` are currently retained as valid unsupported commands, so the capture export is the authoritative place to recover all four responses from the same cycle.

Expected log lines include:

```text
TX OEM request: combined/general fault query -> 0x33
TX OEM request: indoor fault query -> 0x34
TX OEM request: outdoor-unit query -> 0x35
TX OEM request: extended/monthly energy query -> 0x40
```

A unit may ignore a query it does not implement. A missing response is not automatically a parser or wiring failure; compare the checksum, timeout, and ordinary `0x31` polling diagnostics.
