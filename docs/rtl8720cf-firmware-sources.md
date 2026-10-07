# RTL8720CF firmware analysis inputs

The protocol analysis in this branch uses the two archived images below from
`maxim-smirnov/gree-wifimodule-firmware`, firmware code `362001065279`.
The complete upstream repository is also available as a [source archive](https://codeload.github.com/maxim-smirnov/gree-wifimodule-firmware/zip/refs/heads/main).

| Image | Archived protocol | Git blob SHA | Upstream binary | Git object | Upstream metadata |
|---|---|---|---|---|---|
| `U-WB05RT13V1.21-4307516678339185323.bin` | `V2.0.0` | `7c8b864f6d5e621bfc79265ad928f5cc1c8c09a1` | [binary](https://raw.githubusercontent.com/maxim-smirnov/gree-wifimodule-firmware/main/362001065279/U-WB05RT13V1.21-4307516678339185323.bin) | [JSON](https://api.github.com/repos/maxim-smirnov/gree-wifimodule-firmware/git/blobs/7c8b864f6d5e621bfc79265ad928f5cc1c8c09a1) | [metadata](https://raw.githubusercontent.com/maxim-smirnov/gree-wifimodule-firmware/main/362001065279/U-WB05RT13V1.21-4307516678339185323.md) |
| `U-WB05RT13V1.53-17090889809932733253.bin` | `V3.0.0` | `0c0f46e3631fdefa0ce23eb27c14cff0ccb47c3a` | [binary](https://raw.githubusercontent.com/maxim-smirnov/gree-wifimodule-firmware/main/362001065279/U-WB05RT13V1.53-17090889809932733253.bin) | [JSON](https://api.github.com/repos/maxim-smirnov/gree-wifimodule-firmware/git/blobs/0c0f46e3631fdefa0ce23eb27c14cff0ccb47c3a) | [metadata](https://raw.githubusercontent.com/maxim-smirnov/gree-wifimodule-firmware/main/362001065279/U-WB05RT13V1.53-17090889809932733253.md) |

Version 1.21 is retained because its module generation and advertised
communication protocol match the earlier V2 installation context. Version 1.53
is the newest archived image in this firmware-code family and advertises V3.
Both must be analyzed independently before any startup frame, selector, field,
or capability conclusion is applied to the Livo implementation.

The final analysis will add byte size and SHA-256 checksums after independently
verifying the downloaded bytes.
