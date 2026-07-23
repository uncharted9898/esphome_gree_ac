#!/usr/bin/env python3
"""Read-only discovery and full-property probe for Gree Wi-Fi modules.

This utility speaks the local UDP/7000 protocol used by older Gree modules.
It never sends a ``cmd`` pack; only scan, bind, and status requests are used.

The extended property list was recovered from the OEM CS532AX(MTK) firmware.
Many properties are product-family specific. Unsupported properties may be
omitted, return a fixed value, or cause a status request to fail. Failed
chunks are automatically split until the unsupported property is isolated.
"""

from __future__ import annotations

import argparse
import base64
import json
import socket
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

GENERIC_ECB_KEY = b"a3K8Bx%2r8Y7#xDh"
GENERIC_GCM_KEY = b"{yxAHAY_Lm6pbC/<"
GCM_IV = bytes.fromhex("5440784449675a516c5e6313")
GCM_AAD = b"qualcomm-test"
UDP_PORT = 7000

# Ordered table recovered from U-CS532AX(MTK) firmware code 362001000835.
OEM_PROPERTIES: tuple[str, ...] = (
    "Pow", "Mod", "SetTem", "WdSpd", "SwUpDn", "SwingLfRig", "Lig",
    "AssHt", "Blo", "SwhSlp", "Quiet", "Tur", "TemSen", "Emod",
    "SlpMod", "Add0.5", "Air", "AirQ", "DnPLLRswing", "DnPRLRswing",
    "DnPUDSwing", "Health", "HeatCoolType", "HumSen", "PM2P5", "StHt",
    "SvSt", "SwhWifi", "SwhWifiRes", "DsplySt", "TemRec", "TemUn",
    "UDFanport", "Wet", "SaveGuid", "Purify", "FbidBloPer", "EnvArea1St",
    "EnvArea2St", "EnvArea3St", "EnvArea4St", "EnvArea5St", "EnvArea6St",
    "EnvArea7St", "EnvArea8St", "EnvArea9St", "Slp1L1", "Slp1H1",
    "Slp1L2", "Slp1H2", "Slp1L3", "Slp1H3", "Slp1L4", "Slp1H4",
    "Slp1L5", "Slp1H5", "Slp1L6", "Slp1H6", "Slp1L7", "Slp1H7",
    "Slp1L8", "Slp1H8", "NoiseSet", "CoolNoise", "HeatNoise", "RoomLen",
    "RoomWid", "RoomHigh", "ACStupPos", "NobodySave", "SmartWind",
    "HandCtl", "VocCtl", "VocIdion", "VocRole", "Security", "ImgUpdateCol",
    "ImgVerSta", "VocUpdateCol", "VocVerSta", "PMVComfort", "Defrost",
    "Antifreeze", "UnmanedOffTime", "FavorMode", "SecurityMode", "MicroSen",
    "ImgUpdateSta", "ImgUpdateRes", "VocUpdateSta", "VocUpdateRes",
    "ImageRecovery", "ReplaceHEPA", "Dfltr", "UnmanedShutDown", "AllErr",
    "ElcAllKwhClr", "ElcEn",
)


class ProbeError(RuntimeError):
    """Protocol, transport, or decoding failure."""


@dataclass(frozen=True)
class Device:
    ip: str
    port: int
    device_id: str
    name: str
    encryption: str


def _compact_json(value: Any) -> str:
    return json.dumps(value, separators=(",", ":"), ensure_ascii=True)


def _pkcs7_pad(data: bytes) -> bytes:
    padding = 16 - (len(data) % 16)
    return data + bytes([padding]) * padding


def _json_from_decrypted(data: bytes) -> dict[str, Any]:
    # Several firmware generations pad with PKCS#7, 0xFF, or trailing garbage.
    end = data.rfind(b"}")
    if end < 0:
        raise ProbeError("decrypted payload does not contain a JSON object")
    try:
        return json.loads(data[: end + 1].decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProbeError(f"invalid decrypted JSON: {exc}") from exc


def encrypt_ecb(document: dict[str, Any], key: bytes) -> str:
    encryptor = Cipher(algorithms.AES(key), modes.ECB()).encryptor()
    ciphertext = encryptor.update(_pkcs7_pad(_compact_json(document).encode())) + encryptor.finalize()
    return base64.b64encode(ciphertext).decode()


def decrypt_ecb(encoded: str, key: bytes) -> dict[str, Any]:
    decryptor = Cipher(algorithms.AES(key), modes.ECB()).decryptor()
    plaintext = decryptor.update(base64.b64decode(encoded)) + decryptor.finalize()
    return _json_from_decrypted(plaintext)


def encrypt_gcm(document: dict[str, Any], key: bytes) -> tuple[str, str]:
    encrypted = AESGCM(key).encrypt(GCM_IV, _compact_json(document).encode(), GCM_AAD)
    ciphertext, tag = encrypted[:-16], encrypted[-16:]
    return base64.b64encode(ciphertext).decode(), base64.b64encode(tag).decode()


def decrypt_gcm(encoded: str, tag: str, key: bytes) -> dict[str, Any]:
    encrypted = base64.b64decode(encoded) + base64.b64decode(tag)
    plaintext = AESGCM(key).decrypt(GCM_IV, encrypted, GCM_AAD)
    return _json_from_decrypted(plaintext)


def create_outer_request(device_id: str, pack: str, tag: str | None = None, sequence: int = 0) -> bytes:
    request: dict[str, Any] = {
        "cid": "app",
        "i": sequence,
        "t": "pack",
        "uid": 0,
        "tcid": device_id,
    }
    if tag is not None:
        request["tag"] = tag
    request["pack"] = pack
    return _compact_json(request).encode()


def udp_exchange(ip: str, payload: bytes, timeout: float, interface: str | None = None) -> dict[str, Any]:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as sock:
        sock.settimeout(timeout)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if interface and hasattr(socket, "SO_BINDTODEVICE"):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, interface.encode() + b"\0")
        sock.sendto(payload, (ip, UDP_PORT))
        try:
            raw, _ = sock.recvfrom(8192)
        except socket.timeout as exc:
            raise ProbeError(f"timeout waiting for {ip}:{UDP_PORT}") from exc
    try:
        return json.loads(raw[: raw.rfind(b"}") + 1])
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProbeError(f"invalid outer JSON response from {ip}: {exc}") from exc


def scan(broadcast: str, timeout: float, interface: str | None = None) -> list[Device]:
    devices: dict[tuple[str, str], Device] = {}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as sock:
        sock.settimeout(0.2)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        if interface and hasattr(socket, "SO_BINDTODEVICE"):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, interface.encode() + b"\0")
        sock.sendto(b'{"t":"scan"}', (broadcast, UDP_PORT))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                raw, address = sock.recvfrom(8192)
            except socket.timeout:
                continue
            try:
                outer = json.loads(raw[: raw.rfind(b"}") + 1])
                if "tag" in outer:
                    inner = decrypt_gcm(outer["pack"], outer["tag"], GENERIC_GCM_KEY)
                    encryption = "GCM"
                else:
                    inner = decrypt_ecb(outer["pack"], GENERIC_ECB_KEY)
                    encryption = "ECB"
                device_id = str(inner.get("cid") or outer.get("cid") or inner.get("mac") or "")
                if not device_id:
                    continue
                device = Device(
                    ip=address[0],
                    port=address[1],
                    device_id=device_id,
                    name=str(inner.get("name", "<unknown>")),
                    encryption=encryption,
                )
                devices[(device.ip, device.device_id)] = device
            except (KeyError, ValueError, ProbeError, json.JSONDecodeError):
                continue
    return sorted(devices.values(), key=lambda item: (item.ip, item.device_id))


def bind(device: Device, timeout: float, interface: str | None = None) -> tuple[str, str]:
    inner = {"mac": device.device_id, "t": "bind", "uid": 0}
    modes_to_try = [device.encryption] + (["GCM"] if device.encryption == "ECB" else ["ECB"])
    errors: list[str] = []
    for encryption in modes_to_try:
        try:
            if encryption == "GCM":
                pack, tag = encrypt_gcm(inner, GENERIC_GCM_KEY)
                outer = udp_exchange(device.ip, create_outer_request(device.device_id, pack, tag, 1), timeout, interface)
                reply = decrypt_gcm(outer["pack"], outer["tag"], GENERIC_GCM_KEY)
            else:
                pack = encrypt_ecb(inner, GENERIC_ECB_KEY)
                outer = udp_exchange(device.ip, create_outer_request(device.device_id, pack, sequence=1), timeout, interface)
                reply = decrypt_ecb(outer["pack"], GENERIC_ECB_KEY)
            if str(reply.get("t", "")).lower() != "bindok" or not reply.get("key"):
                raise ProbeError(f"unexpected bind reply: {reply}")
            return str(reply["key"]), encryption
        except (KeyError, ProbeError, ValueError) as exc:
            errors.append(f"{encryption}: {exc}")
    raise ProbeError("bind failed; " + "; ".join(errors))


def status_request(
    device: Device,
    key: str,
    encryption: str,
    properties: Sequence[str],
    timeout: float,
    interface: str | None = None,
) -> dict[str, Any]:
    inner = {"cols": list(properties), "mac": device.device_id, "t": "status"}
    key_bytes = key.encode()
    if encryption == "GCM":
        pack, tag = encrypt_gcm(inner, key_bytes)
        outer = udp_exchange(device.ip, create_outer_request(device.device_id, pack, tag), timeout, interface)
        reply = decrypt_gcm(outer["pack"], outer["tag"], key_bytes)
    else:
        pack = encrypt_ecb(inner, key_bytes)
        outer = udp_exchange(device.ip, create_outer_request(device.device_id, pack), timeout, interface)
        reply = decrypt_ecb(outer["pack"], key_bytes)
    if "cols" not in reply or "dat" not in reply:
        raise ProbeError(f"status reply lacks cols/dat: {reply}")
    return reply


def chunks(items: Sequence[str], size: int) -> Iterable[Sequence[str]]:
    for start in range(0, len(items), size):
        yield items[start : start + size]


def query_resilient(
    device: Device,
    key: str,
    encryption: str,
    properties: Sequence[str],
    timeout: float,
    interface: str | None,
    delay: float,
) -> tuple[dict[str, Any], dict[str, str]]:
    values: dict[str, Any] = {}
    errors: dict[str, str] = {}

    def query_group(group: Sequence[str]) -> None:
        if not group:
            return
        try:
            reply = status_request(device, key, encryption, group, timeout, interface)
            returned = dict(zip(reply.get("cols", []), reply.get("dat", [])))
            for prop in group:
                if prop in returned:
                    values[prop] = returned[prop]
                else:
                    errors[prop] = "omitted from status reply"
            if delay:
                time.sleep(delay)
        except (ProbeError, KeyError, ValueError) as exc:
            if len(group) == 1:
                errors[group[0]] = str(exc)
                return
            middle = len(group) // 2
            query_group(group[:middle])
            query_group(group[middle:])

    query_group(properties)
    return values, errors


def load_properties(path: str | None) -> tuple[str, ...]:
    if not path:
        return OEM_PROPERTIES
    entries = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        value = line.strip()
        if value and not value.startswith("#"):
            entries.append(value)
    if not entries:
        raise ProbeError("property file contains no property names")
    return tuple(dict.fromkeys(entries))


def print_snapshot(values: dict[str, Any], errors: dict[str, str], previous: dict[str, Any] | None = None) -> None:
    for prop in OEM_PROPERTIES:
        if prop not in values:
            continue
        marker = ""
        if previous is not None and previous.get(prop) != values[prop]:
            marker = "  CHANGED"
        print(f"{prop:18} = {values[prop]!r}{marker}")
    for prop in sorted(set(values) - set(OEM_PROPERTIES)):
        marker = "  CHANGED" if previous is not None and previous.get(prop) != values[prop] else ""
        print(f"{prop:18} = {values[prop]!r}{marker}")
    if errors:
        print("\nUnsupported/failed properties:", file=sys.stderr)
        for prop, error in errors.items():
            print(f"  {prop}: {error}", file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--broadcast", default="255.255.255.255", help="broadcast address used for scan")
    parser.add_argument("--scan-timeout", type=float, default=5.0)
    parser.add_argument("--timeout", type=float, default=3.0, help="per-request UDP timeout")
    parser.add_argument("--interface", help="Linux interface name for SO_BINDTODEVICE")
    parser.add_argument("--client", help="module IP; skips scan when --id and --key are also supplied")
    parser.add_argument("--id", dest="device_id", help="module CID/MAC")
    parser.add_argument("--key", help="bound 16-byte device key")
    parser.add_argument("--encryption", choices=("ECB", "GCM"), default="ECB")
    parser.add_argument("--device-index", type=int, default=0, help="device selected from scan results")
    parser.add_argument("--chunk-size", type=int, default=20)
    parser.add_argument("--delay", type=float, default=0.05, help="delay between successful status requests")
    parser.add_argument("--properties-file", help="newline-separated replacement property list")
    parser.add_argument("--watch", type=float, default=0.0, metavar="SECONDS", help="repeat and print changed values")
    parser.add_argument("--json-output", help="write the latest snapshot and errors to this path")
    args = parser.parse_args()

    if not 1 <= args.chunk_size <= 48:
        parser.error("--chunk-size must be between 1 and 48")

    try:
        properties = load_properties(args.properties_file)
        if args.client and args.device_id:
            device = Device(args.client, UDP_PORT, args.device_id, "<manual>", args.encryption)
        else:
            devices = scan(args.broadcast, args.scan_timeout, args.interface)
            if not devices:
                raise ProbeError("no Gree modules answered the scan")
            for index, found in enumerate(devices):
                print(f"[{index}] {found.ip}  {found.device_id}  {found.name}  {found.encryption}")
            if args.device_index < 0 or args.device_index >= len(devices):
                raise ProbeError(f"device index {args.device_index} is out of range")
            device = devices[args.device_index]

        key = args.key
        encryption = args.encryption if args.client and args.device_id else device.encryption
        if not key:
            key, encryption = bind(device, args.timeout, args.interface)
            print(f"Bound {device.device_id} at {device.ip}; encryption={encryption}; key={key}")

        if len(key.encode()) != 16:
            raise ProbeError("device key must encode to exactly 16 bytes")

        previous: dict[str, Any] | None = None
        while True:
            values: dict[str, Any] = {}
            errors: dict[str, str] = {}
            for group in chunks(properties, args.chunk_size):
                group_values, group_errors = query_resilient(
                    device, key, encryption, group, args.timeout, args.interface, args.delay
                )
                values.update(group_values)
                errors.update(group_errors)
            print(f"\nSnapshot {time.strftime('%Y-%m-%d %H:%M:%S')} from {device.ip} ({device.device_id})")
            print_snapshot(values, errors, previous)
            if args.json_output:
                Path(args.json_output).write_text(
                    json.dumps(
                        {
                            "timestamp": time.time(),
                            "device": device.__dict__,
                            "encryption": encryption,
                            "values": values,
                            "errors": errors,
                        },
                        indent=2,
                        sort_keys=True,
                    )
                    + "\n",
                    encoding="utf-8",
                )
            if args.watch <= 0:
                break
            previous = values
            time.sleep(args.watch)
    except (OSError, ProbeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
