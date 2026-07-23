# Long-run Gree UART telemetry capture

This workflow is for leaving the installed ESP controller running for several hours or days, then exporting the state-change history from Home Assistant for offline comparison.

It does **not** send any new or guessed supplemental query. Normal climate polling/control remains unchanged. The capture relies on checksum-valid UART reports already observed by the component, including rare valid commands such as `0x33`, `0x40`, `0x44`, and other unsupported commands.

## Why Home Assistant Recorder is used

The ESP32 has limited RAM and flash endurance. Keeping hours of full UART frames in a device-side ring buffer would either lose old data after a short time or require frequent flash writes.

Instead, the component publishes a raw payload text sensor only when that command's payload changes. Home Assistant Recorder timestamps and retains each changed state. A normal `0x31` payload is short enough to fit in a Home Assistant state string.

This means a six-hour run can preserve:

- each distinct `0x31` status payload;
- rare `0x33`, `0x40`, `0x44`, and unknown valid payloads;
- the unresolved payload byte 44 as a normal numeric history series;
- fan-layout diagnostics;
- protocol error counters and communication state.

The capture is change-oriented, not packet-by-packet. Identical 300 ms polling replies are intentionally not duplicated.

## ESPHome configuration

Use [`examples/gree-livo-long-run-telemetry.yaml`](../examples/gree-livo-long-run-telemetry.yaml) as a reference. For the currently installed Livo controller, keep the verified GPIO and UART settings already in use.

Important settings:

```yaml
telemetry_discovery:
  enabled: true
  expose_raw_payload: true
  expose_raw_bytes: false
  log_changes_only: true
  history_depth: 64
```

Expose at least:

```yaml
diagnostics:
  last_0x31_payload:
    name: Gree Last 0x31 Payload
  last_0x33_payload:
    name: Gree Last 0x33 Payload
  last_0x40_payload:
    name: Gree Last 0x40 Payload
  last_0x44_payload:
    name: Gree Last 0x44 Payload
  last_unknown_payload:
    name: Gree Last Unknown Payload
  candidate_telemetry_byte_44_raw:
    name: Gree Candidate Byte 44 Raw
```

Do not enable `supplemental_queries` yet. Captured OEM request templates and repeatable request/response evidence are still required before transmitting anything outside the established poll/control exchange.

## Recorder retention

Home Assistant normally records sensor and text-sensor state changes unless the entity is excluded from Recorder. Verify that the raw payload entities are not excluded by `recorder:` configuration.

Let the device run during normal operation. Useful natural events include:

- compressor starting and stopping;
- room and outdoor temperature changes;
- cool, dry, fan, auto, and heat operation;
- remote-control changes;
- display mode changes;
- quiet, turbo, X-Fan, sleep, and save changes;
- defrost or protection behavior;
- power interruption and startup.

Make a note of approximate times for deliberate changes. Those timestamps make byte correlation much faster.

## Exporting the last several hours

Create a Home Assistant long-lived access token and keep it outside the repository.

```bash
export HA_URL='http://homeassistant.local:8123'
export HA_TOKEN='REDACTED_LONG_LIVED_TOKEN'

python3 tools/export_ha_telemetry_history.py \
  --hours 8 \
  --match 'Last 0x31 Payload' \
  --match 'Last 0x33 Payload' \
  --match 'Last 0x40 Payload' \
  --match 'Last 0x44 Payload' \
  --match 'Last Unknown Payload' \
  --match 'Candidate Byte 44' \
  --output-prefix captures/livo-overnight
```

Exact entity IDs can be used instead of friendly-name matching:

```bash
python3 tools/export_ha_telemetry_history.py \
  --hours 8 \
  --entity text_sensor.gree_livo_telemetry_last_0x31_payload \
  --entity text_sensor.gree_livo_telemetry_last_0x33_payload \
  --entity text_sensor.gree_livo_telemetry_last_0x44_payload
```

The script is read-only and uses only Python's standard library.

## Output files

For an output prefix of `captures/livo-overnight`, the exporter writes:

- `livo-overnight.json`: untouched Home Assistant history response and query metadata;
- `livo-overnight-states.csv`: flattened entity state changes;
- `livo-overnight-payloads.csv`: payloads expanded into `b000`, `b001`, ... columns;
- `livo-overnight-byte-stats.csv`: sample count, change count, min/max/latest value, and changed-bit mask for each byte.

The payload CSV also contains `changed_indexes`, which identifies every byte that changed from the prior recorded payload for the same entity/command.

## Current high-value byte candidates

For the normal `0x31` stripped payload indexing used by this component:

- byte 24: older research suggests an I-Feel/remote-sensor temperature candidate;
- byte 37: older captures suggest possible IR-reception or communication activity;
- byte 42: confirmed indoor/return-air temperature, `(raw - 16) / 2`;
- byte 44: unresolved on Livo Gen3 and observed changing, unlike older captures where the equivalent byte remained zero.

These are research targets, not finalized entity names. The byte-statistics output should be compared with deliberate event timestamps before assigning physical meanings.

## Rare command limitation

A `last_0x33_payload` or similar text sensor records the most recent **changed** state of that command. If no such command appears during the run, the unit did not emit it in response to the established polling traffic during that window.

That absence does not prove the unit lacks the data. It may require an OEM startup exchange, time-sync packet, Wi-Fi-state packet, or another captured request that is not yet reproduced by the ESP component.
