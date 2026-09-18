#!/usr/bin/env python3
"""Recover and compare the RTL8720CF GREE property registry.

The property-name strings live in the 0x9B800140 XIP section while the ordered
pointer registry lives in the 0x10000480 SRAM image.  The registry index is the
same index used by the firmware's uint16 property-value array, so each entry's
value offset is ``index * 2``.
"""

from __future__ import annotations

import argparse
import json
import struct
from dataclasses import asdict, dataclass
from pathlib import Path

from rtl8720cf_image import iter_printable_strings, iter_sections, parse_firmware


@dataclass(frozen=True)
class PropertyEntry:
    index: int
    value_offset: int
    pointer_address: int
    name_address: int
    name: str


@dataclass(frozen=True)
class PropertyRegistry:
    firmware: str
    table_address: int
    entries: tuple[PropertyEntry, ...]


def _sections_by_base(data: bytes, path: str):
    manifest = parse_firmware(data, path)
    return manifest, {section.entry.section_base: section for section in iter_sections(manifest)}


def recover_registry(path: Path) -> PropertyRegistry:
    data = path.read_bytes()
    manifest, sections = _sections_by_base(data, str(path))
    sram = sections.get(0x10000480)
    if sram is None:
        raise ValueError(f"{path}: missing SRAM section at 0x10000480")

    load_sections = tuple(iter_sections(manifest))

    def address_to_file_offset(address: int) -> int | None:
        for section in load_sections:
            if section.entry.section_base <= address < section.vma_end:
                return section.image_offset + (address - section.entry.section_base)
        return None

    def cstring(address: int) -> str | None:
        offset = address_to_file_offset(address)
        if offset is None:
            return None
        end = data.find(b"\0", offset, min(len(data), offset + 128))
        if end < 0 or end == offset:
            return None
        raw = data[offset:end]
        if any(value < 0x20 or value > 0x7E for value in raw):
            return None
        return raw.decode("ascii")

    def suffix_addresses(text: str) -> list[int]:
        needle = text.encode() + b"\0"
        result = []
        for section in load_sections:
            raw = data[section.image_offset : section.image_end]
            cursor = 0
            while True:
                offset = raw.find(needle, cursor)
                if offset < 0:
                    break
                result.append(section.entry.section_base + offset)
                cursor = offset + 1
        return result

    required = {name: suffix_addresses(name) for name in ("Pow", "Mod", "SetTem")}
    if any(not addresses for addresses in required.values()):
        raise ValueError(f"{path}: property registry anchor strings missing: {required}")

    raw = data[sram.image_offset : sram.image_end]
    offset = -1
    for pow_address in required["Pow"]:
        for mod_address in required["Mod"]:
            for settem_address in required["SetTem"]:
                signature = struct.pack("<III", pow_address, mod_address, settem_address)
                candidate = raw.find(signature)
                if candidate >= 0:
                    offset = candidate
                    break
            if offset >= 0:
                break
        if offset >= 0:
            break
    if offset < 0:
        raise ValueError(f"{path}: property registry signature not found in SRAM")

    table_address = sram.entry.section_base + offset
    entries: list[PropertyEntry] = []
    cursor = offset
    while cursor + 4 <= len(raw) and len(entries) < 1024:
        pointer = struct.unpack_from("<I", raw, cursor)[0]
        name = cstring(pointer)
        if name is None:
            break
        index = len(entries)
        entries.append(
            PropertyEntry(
                index=index,
                value_offset=index * 2,
                pointer_address=sram.entry.section_base + cursor,
                name_address=pointer,
                name=name,
            )
        )
        cursor += 4

    if len(entries) < 100:
        raise ValueError(f"{path}: implausibly short property registry ({len(entries)})")
    return PropertyRegistry(str(path), table_address, tuple(entries))


def render_markdown(v2: PropertyRegistry, v3: PropertyRegistry) -> str:
    lines = [
        "# RTL8720CF property registry comparison",
        "",
        f"- V2 table: `0x{v2.table_address:08X}`, {len(v2.entries)} entries",
        f"- V3 table: `0x{v3.table_address:08X}`, {len(v3.entries)} entries",
        "",
        "| Index | Value offset | V2 property | V3 property |",
        "|---:|---:|---|---|",
    ]
    count = max(len(v2.entries), len(v3.entries))
    for index in range(count):
        left = v2.entries[index].name if index < len(v2.entries) else ""
        right = v3.entries[index].name if index < len(v3.entries) else ""
        marker_left = f"**{left}**" if left != right else left
        marker_right = f"**{right}**" if left != right else right
        lines.append(f"| {index} | `0x{index * 2:03X}` | {marker_left} | {marker_right} |")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v2", type=Path)
    parser.add_argument("v3", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--markdown", type=Path)
    args = parser.parse_args()

    v2 = recover_registry(args.v2)
    v3 = recover_registry(args.v3)
    result = {"v2": asdict(v2), "v3": asdict(v3)}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2) + "\n")
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(render_markdown(v2, v3))
    if not args.json and not args.markdown:
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
