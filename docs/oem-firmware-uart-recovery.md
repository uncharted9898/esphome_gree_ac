# CS532AX/MT7687 OEM UART request recovery

> **Historical firmware family.** These findings describe the CS532AX/MT7687
> image and are retained as provenance. The Livo implementation now uses the
> independently audited RTL8720CF V2/V3 request format and field map in
> [`rtl8720cf-telemetry-map.md`](rtl8720cf-telemetry-map.md). Do not copy the
> 28-byte `LEN=0x19` vectors below into the active probe.

This document records request builders recovered from the public Gree
`U-CS532AX(MTK)V3` firmware by static analysis. Byte indexes below are full-frame
indexes including the two `0x7E` sync bytes.

The firmware image is loaded as ARM Cortex-M4 code at `0x1007C000`. The frame
constructors, parser, and response scheduler were decompiled with Ghidra. These
findings are substantially stronger than naming packets from their observed
length alone, but hardware probing should still remain opt-in.

## Frame format confirmed by firmware

```text
7E 7E  LENGTH  COMMAND  PAYLOAD...  CHECKSUM
```

`total frame bytes = LENGTH + 3` and `CHECKSUM` is the low byte of the sum from
`LENGTH` through the last payload byte.

## Full MAC report (`command 0x04`)

Firmware builder: `FUN_1008434a` at `0x1008434A`.

```text
7E 7E 0C 04 07 00 00 00 MM MM MM MM MM MM CS
```

The six `MM` bytes are the module MAC address. The checksum is:

```text
CS = (0C + 04 + 07 + sum(MAC bytes)) & FF
```

This is the full report. The previously captured short startup request
`7E 7E 05 04 07 00 00 10` uses the same command but does not carry the MAC.

## Extended query/status request (`command 0x03`, length `0x19`)

Firmware builder: `FUN_10084394` at `0x10084394`.
Selector writer: `FUN_1008443c` at `0x1008443C`.
Checksum writer: `FUN_10084382` at `0x10084382`.

Base shape:

```text
index  value / role
0-1    7E 7E
2      19
3      03
4      module-state flags in bits 7:6; primary selector in bits 2:0
5-7    00
8-9    range/state fields, normally 00
10     3B sentinel
11-12  range/state fields, normally 00
13     3B sentinel
14     extended selector in bits 2:0
15-25  00 in the recovered query paths
26     current Wi-Fi/module state enum
27     checksum
```

The firmware proves this command is a multiplexed status/query frame. It is not
simply a wall-clock synchronization packet.

### Recovered selectors and responses

| Logical request | Byte 4 low bits | Byte 14 low bits | Expected response |
|---|---:|---:|---:|
| Current/periodic status; used by the firmware's power/energy upload path | `0` | `0` | `0x31` |
| Combined/general fault data | `1` | `0` | `0x33` |
| Indoor-unit fault data | `2` | `0` | `0x34` |
| Outdoor-unit fault data | `4` | `0` | `0x35` |
| Monthly/extended energy data | `0` | `4` | `0x40` |

The selector-to-fault mapping is directly corroborated by firmware log strings
on the three request paths: `fault inner outer`, `fault inner`, and
`fault_outer`.

With module-state byte 26 set to `0x01` and no high module-state flags, static
checksum-valid research vectors are:

```text
combined/general fault -> 0x33
7E 7E 19 03 01 00 00 00 00 00 3B 00 00 3B 00 00 00 00 00 00 00 00 00 00 00 00 01 94

indoor fault -> 0x34
7E 7E 19 03 02 00 00 00 00 00 3B 00 00 3B 00 00 00 00 00 00 00 00 00 00 00 00 01 95

outdoor fault/data -> 0x35
7E 7E 19 03 04 00 00 00 00 00 3B 00 00 3B 00 00 00 00 00 00 00 00 00 00 00 00 01 97

monthly/extended energy -> 0x40
7E 7E 19 03 00 00 00 00 00 00 3B 00 00 3B 04 00 00 00 00 00 00 00 00 00 00 00 01 97
```

These vectors reproduce the builder with a neutral flag set and state enum 1.
The original module may alter byte 4 bits 7:6 and byte 26 according to its Wi-Fi
connection state. The indoor PCB may ignore those module-only fields, but that
must be established experimentally before enabling unattended periodic probes.

## Incoming response parser recovered

Firmware parser: `FUN_100824f2` at `0x100824F2`.

| Incoming command | Firmware treatment |
|---:|---|
| `0x31` | Normal status; compare/update main status structure |
| `0x32` | Initial synchronization response |
| `0x33` | Combined/general fault response |
| `0x34` | Indoor fault response |
| `0x35` | Outdoor fault response |
| `0x40` | Extended/monthly energy response |
| `0x44` | Device/basic-information startup response |
| `0x45` | SSID provisioning acknowledgement |
| `0x46` | Password provisioning acknowledgement |
| `0x47` | Device-information response |

The `0x33`/`0x34`/`0x35` frames are requested together by the OEM firmware and
then packaged into one fault upload. The `0x40` frame is requested separately
and logged by the firmware as the monthly energy report.

## Energy telemetry

There are two relevant paths:

1. A selector-zero long `0x03` request receives ordinary `0x31` and feeds the
   firmware's normal power/energy upload path.
2. Selector 8 receives command `0x40`; its code path logs
   `p_month_frame_verify` and `energy_report_start`, establishing it as the
   extended/monthly energy report.

Therefore `0x40` is not requested by sending command `0x40`. It is the response
to the long command-`0x03` request with extended selector bit `0x04` at full
frame byte 14.

## Timer requests

No dedicated appliance-UART timer-report request was found in this firmware.
Strings such as `1_timer_type`, `2_timer_type`, `3_timer_type`, and
`4_timer_type` are internal Wi-Fi-module/cloud upload scheduler categories.
They select which cloud report routine runs; they are not UART command IDs.

The appliance's on/off timer state remains embedded in the ordinary status
report on units that implement it. Older captures place those fields around
full-frame bytes 17-19, but their encoding still needs model-specific testing.

## Clock/time synchronization correction

The earlier component name `CMD_OUT_SYNC_TIME` is too specific. The recovered
long command-`0x03` builder carries status/query selectors and module-state
fields. It does not contain an absolute year/month/day/hour/minute/second clock.
The firmware's `Time Clear` and timestamp synchronization messages belong to
its cloud/upload timestamp handling.

No separate absolute-clock UART request builder was found. Until a real OEM
capture proves otherwise, command `0x03` should be described as link/extended
status rather than wall-clock synchronization.

## Other command builders found but not telemetry probes

| Outgoing command | Purpose recovered from firmware |
|---:|---|
| `0x05` | SSID provisioning |
| `0x06` | Wi-Fi password provisioning |
| `0x0B` | Cloud access-key/feed-ID provisioning |
| `0x0A` | Short control/acknowledgement helper |

These should not be used for telemetry discovery.

## Recommended hardware sequence

The first controlled experiment should keep normal climate polling disabled
while transmitting one query at a time:

1. selector 1 and retain `0x33`;
2. selector 2 and retain `0x34`;
3. selector 3 and retain `0x35`;
4. selector 8 and retain `0x40`;
5. restore normal `0x01` polling/control.

Do not repeat fault or energy probes at the normal 300 ms polling frequency.
A single startup pass is sufficient for the first test. Later scheduling should
run through the climate component's request lifecycle so it cannot collide with
normal polls or control transactions.
