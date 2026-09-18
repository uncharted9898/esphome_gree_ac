#!/usr/bin/env python3
"""Compare two RTL8720CF/AmebaZ2 firmware images for Gree UART behavior."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from dataclasses import asdict
from pathlib import Path
from typing import Iterable

from rtl8720cf_image import (
    FirmwareManifest,
    GreeFrame,
    PrintableString,
    iter_gree_frames,
    iter_printable_strings,
    iter_sections,
    manifest_to_dict,
    parse_firmware,
)

DEFAULT_KEYWORDS = (
    "uart",
    "serial",
    "energy",
    "electric",
    "power",
    "watt",
    "kwh",
    "current",
    "amp",
    "voltage",
    "volt",
    "compressor",
    "comp",
    "frequency",
    "freq",
    "hz",
    "eev",
    "exv",
    "valve",
    "load",
    "service",
    "diagnostic",
    "diag",
    "fault",
    "error",
    "status",
    "report",
    "selector",
    "query",
    "gree",
    "gatf",
    "gatr",
    "gatd",
    "ghex",
)


GREE_APPLICATION_MARKERS = (
    b"gree_init_thread",
    b"gree_uart_init",
    b"GREE_APLICATION_START",
    b"ElcEn",
    b"CompressorFqy",
    b"GATD=1 Open Tx Printf",
)


def _application_sections(data: bytes, manifest: FirmwareManifest):
    """Select the XIP section that contains the GREE appliance application.

    The archived RTL8720CF images contain two XIP sections.  The section at
    ``0x9B000140`` is the Realtek platform image; the appliance/cloud/UART
    implementation, property dictionaries, and report handlers are in the
    later section at ``0x9B800140``.  Select by content rather than address so
    future images fail loudly instead of silently auditing the wrong section.
    """

    xip_sections = [
        section
        for section in iter_sections(manifest)
        if section.section_type_name == "xip"
    ]
    if not xip_sections:
        return tuple(iter_sections(manifest))

    scored = []
    for section in xip_sections:
        raw = data[section.image_offset : section.image_end]
        score = sum(marker in raw for marker in GREE_APPLICATION_MARKERS)
        scored.append((score, section.entry.section_base, section))
    scored.sort(reverse=True, key=lambda item: (item[0], item[1]))
    best_score, _, best = scored[0]
    if best_score == 0:
        raise ValueError("unable to identify GREE application XIP section")
    return (best,)


def _section_ranges(data: bytes, manifest: FirmwareManifest) -> set[tuple[int, int]]:
    return {
        (section.image_offset, section.image_end)
        for section in _application_sections(data, manifest)
    }


def _in_application(data: bytes, manifest: FirmwareManifest, offset: int) -> bool:
    return any(
        start <= offset < end
        for start, end in _section_ranges(data, manifest)
    )


def _filter_strings(
    strings: Iterable[PrintableString], keywords: tuple[str, ...]
) -> list[PrintableString]:
    return [
        item
        for item in strings
        if any(keyword in item.text.lower() for keyword in keywords)
    ]


def _frame_key(frame: GreeFrame) -> tuple[int, str]:
    return frame.command, frame.raw_hex


def _manifest_summary(manifest: FirmwareManifest) -> dict:
    sections = []
    for image in manifest.sub_images:
        for section in image.sections:
            sections.append(
                {
                    "image_index": image.index,
                    "image_type": image.image_type_name,
                    "section_index": section.index,
                    "section_type": section.section_type_name,
                    "file_offset": section.image_offset,
                    "image_length": section.image_length,
                    "section_base": section.entry.section_base,
                    "entry_address": section.entry.entry_address,
                    "sha256": section.image_sha256,
                }
            )
    return {
        "path": manifest.path,
        "size": manifest.size,
        "sha256": manifest.sha256,
        "sections": sections,
    }


def analyze(path: Path, keywords: tuple[str, ...]) -> dict:
    data = path.read_bytes()
    manifest = parse_firmware(data, str(path))
    all_strings = list(iter_printable_strings(data, manifest, minimum_length=4))
    strings = [
        item
        for item in all_strings
        if _in_application(data, manifest, item.file_offset)
    ]
    matched = _filter_strings(strings, keywords)
    frames = [
        frame
        for frame in iter_gree_frames(data, manifest)
        if _in_application(data, manifest, frame.file_offset)
    ]
    return {
        "manifest": _manifest_summary(manifest),
        "matched_strings": [asdict(item) for item in matched],
        "embedded_frames": [asdict(item) for item in frames],
        "command_counts": dict(sorted(Counter(frame.command for frame in frames).items())),
        "all_string_count": len(strings),
    }


def compare(left: dict, right: dict) -> dict:
    left_strings = {item["text"] for item in left["matched_strings"]}
    right_strings = {item["text"] for item in right["matched_strings"]}
    left_frames = {
        (item["command"], item["raw_hex"]): item for item in left["embedded_frames"]
    }
    right_frames = {
        (item["command"], item["raw_hex"]): item for item in right["embedded_frames"]
    }
    return {
        "strings_only_in_left": sorted(left_strings - right_strings),
        "strings_only_in_right": sorted(right_strings - left_strings),
        "shared_strings": sorted(left_strings & right_strings),
        "frames_only_in_left": [
            left_frames[key] for key in sorted(left_frames.keys() - right_frames.keys())
        ],
        "frames_only_in_right": [
            right_frames[key] for key in sorted(right_frames.keys() - left_frames.keys())
        ],
        "shared_frames": [
            left_frames[key] for key in sorted(left_frames.keys() & right_frames.keys())
        ],
    }


def render_markdown(left_name: str, right_name: str, result: dict) -> str:
    left = result["left"]
    right = result["right"]
    diff = result["diff"]

    def section_table(item: dict) -> str:
        rows = [
            "| Image | Type | File offset | VMA | Length | SHA-256 |",
            "|---:|---|---:|---:|---:|---|",
        ]
        for section in item["manifest"]["sections"]:
            rows.append(
                "| {image_index} | {image_type}/{section_type} | `0x{file_offset:X}` | "
                "`0x{section_base:08X}` | `0x{image_length:X}` | `{sha256}` |".format(
                    **section
                )
            )
        return "\n".join(rows)

    lines = [
        "# RTL8720CF firmware comparison",
        "",
        f"## {left_name}",
        "",
        f"- File size: `{left['manifest']['size']}` bytes",
        f"- SHA-256: `{left['manifest']['sha256']}`",
        "",
        section_table(left),
        "",
        f"## {right_name}",
        "",
        f"- File size: `{right['manifest']['size']}` bytes",
        f"- SHA-256: `{right['manifest']['sha256']}`",
        "",
        section_table(right),
        "",
        "## Embedded checksum-valid Gree UART frames",
        "",
        f"- {left_name}: `{len(left['embedded_frames'])}`",
        f"- {right_name}: `{len(right['embedded_frames'])}`",
        f"- Shared exact frames: `{len(diff['shared_frames'])}`",
        f"- Only in {left_name}: `{len(diff['frames_only_in_left'])}`",
        f"- Only in {right_name}: `{len(diff['frames_only_in_right'])}`",
        "",
        "## Protocol-relevant strings",
        "",
        f"- Only in {left_name}: `{len(diff['strings_only_in_left'])}`",
        f"- Only in {right_name}: `{len(diff['strings_only_in_right'])}`",
        f"- Shared: `{len(diff['shared_strings'])}`",
        "",
    ]
    for heading, key in (
        (f"Strings only in {left_name}", "strings_only_in_left"),
        (f"Strings only in {right_name}", "strings_only_in_right"),
    ):
        lines.extend([f"### {heading}", ""])
        values = diff[key]
        if values:
            lines.extend(f"- `{value}`" for value in values)
        else:
            lines.append("- None")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--left-name", default="v1.21/V2")
    parser.add_argument("--right-name", default="v1.53/V3")
    parser.add_argument("--json", type=Path, required=True)
    parser.add_argument("--markdown", type=Path, required=True)
    parser.add_argument(
        "--keyword",
        action="append",
        default=[],
        help="additional case-insensitive string keyword",
    )
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    keywords = tuple(dict.fromkeys((*DEFAULT_KEYWORDS, *[k.lower() for k in args.keyword])))
    left = analyze(args.left, keywords)
    right = analyze(args.right, keywords)
    result = {
        "keywords": keywords,
        "left": left,
        "right": right,
        "diff": compare(left, right),
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(result, indent=2) + "\n")
    args.markdown.write_text(
        render_markdown(args.left_name, args.right_name, result)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
