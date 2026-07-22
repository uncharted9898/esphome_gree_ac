# ESPHOME component to support Gree/Sinclair AC units
This repository adds support for ESP32-based WiFi modules to interface with Gree/Sinclair AC units.
This generally replaces stock WiFi module, sometimes giving a little more advanced features for swing control than stock application or remote.

**USE AT YOUR OWN RISK!**

Work is still in progress!

Tested with Sinclair AC (MV-H09BIF), mostly works, sometimes need to send parameter change twice - need to investigate.

Communication protocol is based on my own reverse-engineering.

ESPHome interface/binding based on:
* https://github.com/DomiStyle/esphome-panasonic-ac

**USAGE**
* Use at your own risk!
* See: https://github.com/piotrva/esphome_gree_ac/tree/main/examples
* Create configuration file: `ac-sinclair-main.yaml`
* Configure youe ESP `board`, `uart`, optionally `status_led`, check `wifi` settings (secrets)
* Create configuration(s) for your device(s): `ac-living-room.yaml`, `ac-bedroom.yaml`
* Configure deviceid and devicename, use proper `api` and `ota` keys
* Upload initial configuration to your ESP board using USB connection
* Disconnect completely power from your AC system, follow all safety procedures, desolder original WiFI unit
* Prepare a DIY adapter to connect ESP board to the AC unit, see table and representative schematic below
* Reconnect power to your AC system.
* Enjoy!

Generally stock WiFi module outputs UART with 3.3V signal levels and the AC unit outputs UART with 5V signal levels therefore a simple voltage divider on UART from AC unit towards ESP is usually suitable, considering very slow baudrate.
On some stock WiFi PCBs AC unit connector pins are marked on silkscreen.

| AC unit pin | Function | ESP connection        |
| ----------- | -------- | --------------------- |
| 1           | +5V      | VIN / 5V              |
| 2           | RX       | UART TX               |
| 3           | TX       | UART RX (via divider) |
| 4           | GND      | GND                   |

![Connection schematic](./images/schematic.png)

**NOTES**
* It was reported [#1](https://github.com/piotrva/esphome_gree_ac/issues/1) that with some changes the code works with Lennox li024ci AC

**TODO**
* Support Timers - maybe unnecessray as timers can be managed by Home Assistant
* Support Time sync - maybe unnecessray as timers can be managed by Home Assistant

## Receive-only compatibility and protocol discovery

The `sinclair_ac` component can safely observe candidate Gree Livo Gen3 and Gen4
units before it controls them. Set `transmit_enabled: false`; this is a software
TX lock and no UART write path (including polling or state packets) is used. Keep
the physical TX wire disconnected during initial work as an additional safeguard.
See [`examples/gree-livo-gen3-debug.yaml`](examples/gree-livo-gen3-debug.yaml).

Known `0x31` reports decode the existing Sinclair-compatible common prefix. Valid
frames with other command bytes are retained, counted, and optionally logged; they
never update climate state or cause a response. Additional bytes in a longer report
remain visible in raw packet logs, but are not assigned inferred meanings.

Before connecting any revision, verify connector orientation and logic/power
voltages. In particular, **never connect USB 5 V and HVAC 5 V simultaneously**.
After observing stable traffic and verifying pinout/voltage, explicitly enable TX
and reconnect it only when ready for controlled testing.

Useful captures include startup, power on/off, every operating and fan mode, each
vane position, display changes, sleep, X-Fan, save/8 °C heat, IR remote changes,
and fault states. Submit the labelled packet logs or serial captures with all Wi-Fi
credentials, API keys, MAC addresses, and location details removed. Climate action
is inferred from selected mode and room/target temperatures; it is not a compressor
running indication.
