# Captured OEM boot-sequence probe

This opt-in experiment replays the byte-for-byte startup requests published for original Gree CS532-family Wi-Fi modules before returning the installed ESP controller to normal operation.

It is intended to answer one narrow question: does the indoor unit expose additional reports only after the original module's initialization exchange?

## Captured sequence

The public capture describes this startup behavior:

| Outgoing request | Expected incoming family | Repetitions |
| --- | --- | ---: |
| length `0x10`, command `0x02` | length `0x03`, command `0x32` | 1 |
| length `0x05`, command `0x04` | length `0x1A`, command `0x44` | 2 |
| length `0x0E`, command `0x03` | length `0x2F`, commonly command `0x33` | 4 |
| normal length `0x2F`, command `0x01` polling | length `0x31`, command `0x31` | ongoing |

The probe embeds only the exact checksum-valid request examples. It does not synthesize fields or issue writes to HVAC settings.

## Safety and scheduling

The climate platform must start with:

```yaml
protocol_mode: receive_only
```

The probe then:

1. keeps the climate component receive-only;
2. sends the seven captured startup frames at a conservative 450 ms spacing;
3. lets the existing parser retain all checksum-valid replies;
4. changes the climate component to normal `control` mode after the sequence.

Normal polling and control cannot collide with the startup requests while the probe owns the UART.

## Example

Use [`examples/gree-livo-oem-boot-probe.yaml`](../examples/gree-livo-oem-boot-probe.yaml). The important configuration is:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/uncharted9898/esphome_gree_ac
      ref: research/oem-boot-sequence-probe
    components: [sinclair_ac, gree_oem_boot_probe]

climate:
  - platform: sinclair_ac
    id: gree_ac
    protocol_mode: receive_only
    telemetry_discovery:
      enabled: true
      expose_raw_payload: true
      log_changes_only: true

gree_oem_boot_probe:
  uart_id: gree_uart
  climate_id: gree_ac
  frame_spacing: 450ms
  restore_control: true
```

## What to look for after reboot

Home Assistant Recorder should preserve changed states from:

- `Last 0x44 Payload` after the two `0x05/0x04` requests;
- `Last 0x33 Payload` or another valid unknown response after `0x0E/0x03`;
- `Last Unknown Payload` for response commands not currently named;
- the subsequent `0x31` payload, especially bytes that differ from the pre-probe baseline.

The ESP logs should show seven lines beginning with:

```text
TX captured OEM request:
```

followed by:

```text
Captured OEM boot sequence complete; normal climate control enabled
```

## Interpreting a quiet result

No new report does not prove that the unit lacks extra telemetry. The published `0x10` request contains date/time-looking bytes from one specific capture, and the `0x0E` request contains a Wi-Fi-connected-state flag. Some units may require different identity, MAC, clock, or connection-state values.

The next refinement should come from passive capture of this exact CS532AE module rather than modifying those fields by guesswork.
