# RTL8720CF command `0x44` startup handshake

This document records the command-`0x44` startup behavior recovered independently
from these two archived RTL8720CF images:

- `U-WB05RT13V1.21-4307516678339185323.bin` (`commProtVer` V2)
- `U-WB05RT13V1.53-17090889809932733253.bin` (`commProtVer` V3)

Both versions implement the same standard `0x44` field layout, acceptance gate,
full-MAC command-`0x04` retry loop, and absence of an appliance-UART `0x47`
handler.

The principal recovered functions are:

| Function | V2 v1.21 | V3 v1.53 |
|---|---:|---:|
| appliance-UART receive dispatcher | `0x9B0274DC` | `0x9B02C794` |
| command-`0x44` field decoder | `0x9B026B38` | `0x9B02BB78` |
| startup transmit scheduler | `0x9B026DB8` | `0x9B02BDF8` |
| full-MAC command-`0x04` builder | `0x9B02D5BC` | `0x9B032AA8` |
| cloud device-information dispatcher | `0x9B02A4C0` | `0x9B02FCE4` |

## Standard `0x44` response

The normal response has 24 payload bytes:

```text
7E 7E 1A 44
01 00 01 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 01
CS
```

The payload is not a set of 24 independent switches. It contains three fields:

| Payload byte(s) | Firmware interpretation | Captured value |
|---:|---|---:|
| `0..3` | Device MID, unsigned 32-bit little-endian | `0x00010001` |
| `4..19` | 16-byte binding code (`bc`), preserved as a 32-character hexadecimal string | all zero |
| `20..23` | `VendorInt`, unsigned 32-bit big-endian | `1` |

Therefore the three visible `0x01` bytes do **not** enable three later
transactions. The first two are constituent bytes of one MID integer; the last
is the low byte of the big-endian vendor integer.

The standard frame is accepted when both MID and `VendorInt` are non-zero. The
binding code may be all zero. For the captured payload, both acceptance values
are non-zero, so V2 and V3 advance the startup state normally.

## Optional extended form

If bytes exist after the standard 24-byte payload, the first extension byte is
inspected separately. An extension marker of `0xC9`, combined with configuration
mode 4, selects the firmware's extended state 8 path. Other extended layouts
are parsed into additional internal identification strings. None of those
bytes are present in the captured 24-byte response and they must not be inferred
from it.

## Code path after receiving `0x44`

The V2 and V3 receive dispatchers perform the same sequence:

1. Retain the complete command-`0x44` frame excluding its checksum.
2. Clear a related pending-state field.
3. Call the dedicated `0x44` decoder.
4. Decode and store MID.
5. Decode and store `VendorInt`.
6. Copy the 16-byte binding code to its hexadecimal cloud-property form.
7. Set the startup/retry state to 6 when MID and `VendorInt` are valid.
8. Optionally select state 8 for the `0xC9` extended form.

The accepted standard response therefore ends the command-`0x04` identification
retry phase. It does not request another command-`0x04` transmission.

## Initial command `0x02`

Both audited RTL versions build the same 19-byte identity frame:

```text
7E 7E 10 02 00 00 00 00 00 00 03 00 28 1E 19 23 23 00 BA
```

The byte at full-frame offset 10 is `0x03`. The earlier probe's `0x01` value
and resulting `0xB8` checksum came from the older experimental sequence and do
not match either RTL8720CF image.

## Full MAC-bearing command `0x04`

Both RTL versions build this 16-byte frame from the Wi-Fi module's real six-byte
MAC address:

```text
7E 7E 0D 04 07 00 00 00 M0 M1 M2 M3 M4 M5 00 CS
```

`LEN=0x0D` counts command `0x04`, its 11-byte payload, and the checksum. The
checksum is the modulo-256 sum from `LEN` through the final reserved zero byte.

The startup transmitter selects this full-MAC frame while its identification
attempt counter is below six. Each transmission increments the counter. A valid
`0x44` response sets the handshake state to 6 and prevents another retry. If all
six attempts complete without a usable MID, the RTL firmware installs fallback
MID `0x00010001` and continues its wider startup scheduler.

Accordingly, the correct order is:

```text
full-MAC 0x04 solicitation/retry -> valid 0x44 -> continue startup
```

not:

```text
valid 0x44 -> another full-MAC 0x04
```

## Follow-on transactions

Commands `0x05` and `0x06` are built by a separate provisioning/configuration
path and are gated by independent internal counters and configuration flags.
They are not enabled by any of the three visible `0x01` bytes in the standard
`0x44` payload.

## Why `0x47` is not the next UART response

The V2 and V3 appliance-UART receive dispatchers contain a real `0x44` case, but
command `0x47` falls through the default/unhandled path. No appliance-UART
builder assigning command byte `0x47` was recovered either. Receiving `0x47`
does not advance the RTL startup state.

The firmware's actual device-information transaction is at the cloud/application
layer:

```json
{"t":"scan"}
```

triggers a response whose type is:

```json
{"t":"dev", "mid": "...", "bc": "...", "vender": "..."}
```

That response incorporates the MID, binding code, and vendor value learned from
`0x44`, together with MAC, model, name, version, and other module properties.
This cloud `scan -> dev` exchange must not be relabeled as appliance-UART command
`0x47` without separate evidence from another firmware family.

## ESPHome behavior

The boot probe now:

1. sends the captured identity frame;
2. generates the exact 16-byte RTL command-`0x04` frame with the ESP's real MAC;
3. waits for a newly received `0x44` payload;
4. validates MID and `VendorInt` using the recovered decoder;
5. retries command `0x04` up to six times only while no valid `0x44` arrives;
6. stops command-`0x04` retransmission immediately after acceptance;
7. never waits for or fabricates command `0x47`.

The pre-existing target-specific link-synchronization frames remain a separate
captured step after the RTL identification gate; they are not presented as part
of the `0x44` field semantics.
