# Extra telemetry discovery with an OEM Gree Wi-Fi module

> **Separate cloud/property experiment.** This tool queries an original module
> over its local UDP interface; it does not prove that every named property is
> present on the indoor appliance UART. For the fields and read-only selectors
> recovered from the replacement module's RTL8720CF UART firmware, see
> [`rtl8720cf-telemetry-map.md`](rtl8720cf-telemetry-map.md).

This research path uses an original Gree Wi-Fi module as a read-only protocol bridge. The module accepts local UDP status requests containing arbitrary property names and translates them into whatever internal appliance communication is required by its firmware.

The included `tools/gree_full_status_probe.py` utility never sends a Gree `cmd` request. It performs only:

1. device scan;
2. local bind;
3. read-only `status` requests.

## Why this matters

The normal HVAC UART `0x31` report exposes known fields such as power, mode, setpoint, fan, swing and indoor temperature. OEM module firmware contains a much larger property dictionary, including candidate values for:

- `TemSen`, `TemRec`, `HumSen`, `Wet`;
- `AirQ`, `PM2P5`;
- `HeatCoolType`, `Defrost`, `Antifreeze`;
- `AllErr`;
- `ElcEn`;
- `NoiseSet`, `CoolNoise`, `HeatNoise`.

A property returned by the module is not automatically proof that the indoor unit supports it. Unsupported properties can be omitted, fixed at a default value, or reject a request. The probe splits failed groups recursively until the failing property is isolated.

## Setup

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install cryptography
```

Connect and pair the OEM Wi-Fi module normally. Do not let the OEM module and a replacement ESP controller transmit on the same appliance UART simultaneously.

## First read-only scan

Use the actual broadcast address for the module's subnet:

```sh
python tools/gree_full_status_probe.py \
  --broadcast 192.168.1.255 \
  --json-output gree-full-status.json
```

The tool displays discovered devices, binds the selected module and queries the OEM property table in small groups.

The bind key printed by the tool is a device credential. Do not publish it in logs, screenshots or issue reports.

## Watch for changing values

```sh
python tools/gree_full_status_probe.py \
  --broadcast 192.168.1.255 \
  --watch 5 \
  --json-output gree-full-status.json
```

Change only one appliance condition at a time and record the time:

- room temperature;
- outdoor temperature;
- compressor start/stop;
- defrost;
- fan speed;
- mode;
- display-temperature selection;
- a deliberately generated, safe service condition when available.

Values that change are marked `CHANGED`.

## Query a known module directly

After a successful bind, later runs can skip discovery:

```sh
python tools/gree_full_status_probe.py \
  --client 192.168.1.50 \
  --id AABBCCDDEEFF \
  --key '16-byte-bind-key' \
  --encryption ECB \
  --watch 5
```

Older modules commonly use ECB. Some later firmware uses GCM; automatic scan/bind handles both.

## Correlating LAN properties with UART captures

The useful experiment is to collect two synchronized streams:

1. OEM module property snapshots from this probe;
2. appliance UART reports captured by an RX-only, high-impedance sniffer.

Do not connect two active transmitters to the appliance UART. A passive sniffer must not drive either line.

For each event, compare property changes against:

- normal `0x31` payload byte changes;
- rare valid commands such as `0x33` or `0x44`;
- the unresolved Livo Gen3 `payload[44]` value.

This separates three cases:

- the value is already present in the normal `0x31` report;
- the module obtains it through a supplemental UART exchange;
- the value exists only in the Wi-Fi/cloud application layer.

## Priority property groups

### Sensors

```text
TemSen TemRec HumSen Wet AirQ PM2P5
```

### Operating state

```text
HeatCoolType Defrost Antifreeze StHt AssHt
```

### Fault and energy

```text
AllErr ElcEn
```

`ElcAllKwhClr` appears in the OEM firmware dictionary but its name suggests a clearing operation. The probe only asks for its status value and never writes it. Do not send it through a `cmd` request during discovery.

## Supplying a smaller custom property list

Create a newline-separated file:

```text
TemSen
TemRec
HumSen
Wet
HeatCoolType
Defrost
AllErr
ElcEn
```

Then run:

```sh
python tools/gree_full_status_probe.py \
  --broadcast 192.168.1.255 \
  --properties-file candidate-properties.txt \
  --watch 5
```
