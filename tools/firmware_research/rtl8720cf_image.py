#!/usr/bin/env python3
"""Inspect Realtek AmebaZ2/RTL8720CF OTA firmware images.

The Gree RTL8720CF images are AmebaZ2 OTA containers.  This module parses the
container without third-party dependencies, extracts loadable sections, maps
file offsets to runtime addresses, and scans application bytes for printable
strings and checksum-valid Gree UART frames.

The implementation follows the public AmebaZ2 structures documented by
LibreTiny/ltchiptool.  It intentionally stops at structural facts; it does not
assign protocol semantics to a byte merely because a value looks plausible.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Iterator, Sequence

OTA_PREFIX_SIZE = 0xE0
IMAGE_HEADER_SIZE = 0x60
FIRMWARE_SECURITY_TABLE_SIZE = 0x60
SECTION_HEADER_SIZE = 0x60
ENTRY_HEADER_SIZE = 0x20
HASH_SIZE = 0x20
MAX_GREE_FRAME_SIZE = 200

IMAGE_TYPE_NAMES = {
    0x00: "partition_table",
    0x01: "boot",
    0x02: "fwhs_secure",
    0x03: "fwhs_nonsecure",
    0x04: "fwls",
    0x05: "isp",
    0x06: "voe",
    0x07: "wlan",
    0x08: "xip",
    0x09: "cpfw",
    0x0A: "wowl",
    0x0B: "cinit",
}

SECTION_TYPE_NAMES = {
    0x80: "dtcm",
    0x81: "itcm",
    0x82: "sram",
    0x83: "psram",
    0x84: "lpddr",
    0x85: "xip",
}


class FirmwareFormatError(ValueError):
    """Raised when an image cannot be parsed safely."""


@dataclass(frozen=True)
class EntryHeader:
    image_length: int
    section_base: int
    entry_address: int


@dataclass(frozen=True)
class Section:
    index: int
    header_offset: int
    size: int
    next_section_header: int
    section_type: int
    section_type_name: str
    entry: EntryHeader
    image_offset: int
    image_length: int
    storage_length: int
    image_sha256: str

    @property
    def image_end(self) -> int:
        return self.image_offset + self.image_length

    @property
    def vma_end(self) -> int:
        return self.entry.section_base + self.image_length

    def contains_file_offset(self, offset: int) -> bool:
        return self.image_offset <= offset < self.image_end

    def file_offset_to_vma(self, offset: int) -> int:
        if not self.contains_file_offset(offset):
            raise ValueError(f"file offset 0x{offset:X} is not in section {self.index}")
        return self.entry.section_base + (offset - self.image_offset)


@dataclass(frozen=True)
class SubImage:
    index: int
    offset: int
    segment_size: int
    next_image: int
    image_type: int
    image_type_name: str
    encrypted: bool
    flags: int
    serial: int
    hash_offset: int
    sections: tuple[Section, ...]


@dataclass(frozen=True)
class PrintableString:
    file_offset: int
    vma: int | None
    text: str


@dataclass(frozen=True)
class GreeFrame:
    file_offset: int
    vma: int | None
    declared_length: int
    command: int
    raw_hex: str


@dataclass(frozen=True)
class FirmwareManifest:
    path: str
    size: int
    sha256: str
    sub_images: tuple[SubImage, ...]


def _u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise FirmwareFormatError(f"read past end of image at 0x{offset:X}")
    return struct.unpack_from("<I", data, offset)[0]


def _align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def parse_firmware(data: bytes, path: str = "<memory>") -> FirmwareManifest:
    """Parse an AmebaZ2 OTA image and return its loadable section map."""

    if len(data) < OTA_PREFIX_SIZE + IMAGE_HEADER_SIZE:
        raise FirmwareFormatError("image is too short for an AmebaZ2 OTA header")

    sub_images: list[SubImage] = []
    sub_offset = OTA_PREFIX_SIZE
    seen_offsets: set[int] = set()

    for image_index in range(64):
        if sub_offset in seen_offsets:
            raise FirmwareFormatError(f"sub-image loop at 0x{sub_offset:X}")
        seen_offsets.add(sub_offset)

        if sub_offset + IMAGE_HEADER_SIZE > len(data):
            raise FirmwareFormatError(f"truncated sub-image header at 0x{sub_offset:X}")

        segment_size = _u32(data, sub_offset)
        next_image = _u32(data, sub_offset + 4)
        image_type = data[sub_offset + 8]
        encrypted = data[sub_offset + 9] != 0
        flags = data[sub_offset + 0x0B]
        serial = _u32(data, sub_offset + 0x14)

        if segment_size in (0, 0xFFFFFFFF):
            raise FirmwareFormatError(
                f"invalid segment size 0x{segment_size:08X} at 0x{sub_offset:X}"
            )

        section_offset = (
            sub_offset + IMAGE_HEADER_SIZE + FIRMWARE_SECURITY_TABLE_SIZE
        )
        segment_end = sub_offset + IMAGE_HEADER_SIZE + segment_size
        if segment_end + HASH_SIZE > len(data):
            raise FirmwareFormatError(
                f"sub-image {image_index} extends past file end: 0x{segment_end:X}"
            )

        sections: list[Section] = []
        for section_index in range(64):
            if section_offset + SECTION_HEADER_SIZE + ENTRY_HEADER_SIZE > segment_end:
                raise FirmwareFormatError(
                    f"truncated section {section_index} at 0x{section_offset:X}"
                )

            size = _u32(data, section_offset)
            next_section_header = _u32(data, section_offset + 4)
            section_type = data[section_offset + 8]
            if size < ENTRY_HEADER_SIZE or size == 0xFFFFFFFF:
                raise FirmwareFormatError(
                    f"invalid section size 0x{size:08X} at 0x{section_offset:X}"
                )

            entry_offset = section_offset + SECTION_HEADER_SIZE
            entry = EntryHeader(
                image_length=_u32(data, entry_offset),
                section_base=_u32(data, entry_offset + 4),
                entry_address=_u32(data, entry_offset + 8),
            )
            image_offset = entry_offset + ENTRY_HEADER_SIZE
            storage_length = size - ENTRY_HEADER_SIZE
            image_length = entry.image_length
            if image_length > storage_length:
                raise FirmwareFormatError(
                    f"entry length 0x{image_length:X} exceeds section storage "
                    f"0x{storage_length:X} at 0x{section_offset:X}"
                )
            if image_offset + image_length > len(data):
                raise FirmwareFormatError(
                    f"section image at 0x{image_offset:X} extends past file end"
                )

            image = data[image_offset : image_offset + image_length]
            sections.append(
                Section(
                    index=section_index,
                    header_offset=section_offset,
                    size=size,
                    next_section_header=next_section_header,
                    section_type=section_type,
                    section_type_name=SECTION_TYPE_NAMES.get(
                        section_type, f"unknown_0x{section_type:02X}"
                    ),
                    entry=entry,
                    image_offset=image_offset,
                    image_length=image_length,
                    storage_length=storage_length,
                    image_sha256=hashlib.sha256(image).hexdigest(),
                )
            )

            if next_section_header == 0xFFFFFFFF:
                break

            sequential = section_offset + SECTION_HEADER_SIZE + _align(size, 0x20)
            if next_section_header == 0:
                section_offset = sequential
            else:
                # Realtek images normally store a relative offset.  Accept the
                # sequential value as a defensive fallback for images whose
                # field is informational rather than authoritative.
                candidate = section_offset + next_section_header
                section_offset = (
                    candidate
                    if section_offset < candidate < segment_end
                    else sequential
                )
        else:
            raise FirmwareFormatError("too many sections in one sub-image")

        sub_images.append(
            SubImage(
                index=image_index,
                offset=sub_offset,
                segment_size=segment_size,
                next_image=next_image,
                image_type=image_type,
                image_type_name=IMAGE_TYPE_NAMES.get(
                    image_type, f"unknown_0x{image_type:02X}"
                ),
                encrypted=encrypted,
                flags=flags,
                serial=serial,
                hash_offset=segment_end,
                sections=tuple(sections),
            )
        )

        if next_image == 0xFFFFFFFF:
            break
        if next_image < IMAGE_HEADER_SIZE:
            raise FirmwareFormatError(
                f"invalid next-image distance 0x{next_image:X} at 0x{sub_offset:X}"
            )
        sub_offset += next_image
    else:
        raise FirmwareFormatError("too many sub-images")

    return FirmwareManifest(
        path=path,
        size=len(data),
        sha256=hashlib.sha256(data).hexdigest(),
        sub_images=tuple(sub_images),
    )


def iter_sections(manifest: FirmwareManifest) -> Iterator[Section]:
    for image in manifest.sub_images:
        yield from image.sections


def file_offset_to_vma(manifest: FirmwareManifest, offset: int) -> int | None:
    for section in iter_sections(manifest):
        if section.contains_file_offset(offset):
            return section.file_offset_to_vma(offset)
    return None


def iter_printable_strings(
    data: bytes,
    manifest: FirmwareManifest,
    *,
    minimum_length: int = 4,
    section_types: Sequence[str] | None = None,
) -> Iterator[PrintableString]:
    """Yield printable ASCII strings from loadable section bytes."""

    allowed = set(section_types) if section_types else None
    for section in iter_sections(manifest):
        if allowed is not None and section.section_type_name not in allowed:
            continue
        raw = data[section.image_offset : section.image_end]
        start: int | None = None
        for index, value in enumerate(raw):
            printable = 0x20 <= value <= 0x7E
            if printable and start is None:
                start = index
            if printable:
                continue
            if start is not None and index - start >= minimum_length:
                file_offset = section.image_offset + start
                yield PrintableString(
                    file_offset=file_offset,
                    vma=section.entry.section_base + start,
                    text=raw[start:index].decode("ascii"),
                )
            start = None
        if start is not None and len(raw) - start >= minimum_length:
            file_offset = section.image_offset + start
            yield PrintableString(
                file_offset=file_offset,
                vma=section.entry.section_base + start,
                text=raw[start:].decode("ascii"),
            )


def iter_gree_frames(
    data: bytes,
    manifest: FirmwareManifest,
    *,
    section_types: Sequence[str] | None = None,
) -> Iterator[GreeFrame]:
    """Yield embedded checksum-valid ``7E 7E`` Gree frames."""

    allowed = set(section_types) if section_types else None
    for section in iter_sections(manifest):
        if allowed is not None and section.section_type_name not in allowed:
            continue
        raw = data[section.image_offset : section.image_end]
        index = 0
        while index + 5 <= len(raw):
            if raw[index : index + 2] != b"\x7E\x7E":
                index += 1
                continue
            declared = raw[index + 2]
            total = declared + 3
            if not (5 <= total <= MAX_GREE_FRAME_SIZE) or index + total > len(raw):
                index += 1
                continue
            frame = raw[index : index + total]
            checksum = sum(frame[2:-1]) & 0xFF
            if checksum != frame[-1]:
                index += 1
                continue
            file_offset = section.image_offset + index
            yield GreeFrame(
                file_offset=file_offset,
                vma=section.entry.section_base + index,
                declared_length=declared,
                command=frame[3],
                raw_hex=" ".join(f"{byte:02X}" for byte in frame),
            )
            index += total


def manifest_to_dict(manifest: FirmwareManifest) -> dict:
    return asdict(manifest)


def extract_sections(data: bytes, manifest: FirmwareManifest, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for image in manifest.sub_images:
        for section in image.sections:
            name = (
                f"image{image.index:02d}_{image.image_type_name}_"
                f"section{section.index:02d}_{section.section_type_name}_"
                f"0x{section.entry.section_base:08X}.bin"
            )
            (output_dir / name).write_bytes(
                data[section.image_offset : section.image_end]
            )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--json", type=Path, help="write manifest JSON")
    parser.add_argument("--extract-dir", type=Path, help="extract all sections")
    parser.add_argument("--strings", type=Path, help="write printable strings JSON")
    parser.add_argument("--frames", type=Path, help="write embedded Gree frames JSON")
    parser.add_argument("--minimum-string-length", type=int, default=4)
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    data = args.firmware.read_bytes()
    manifest = parse_firmware(data, str(args.firmware))
    encoded = json.dumps(manifest_to_dict(manifest), indent=2) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(encoded)
    else:
        print(encoded, end="")

    if args.extract_dir:
        extract_sections(data, manifest, args.extract_dir)
    if args.strings:
        strings = [
            asdict(item)
            for item in iter_printable_strings(
                data,
                manifest,
                minimum_length=args.minimum_string_length,
            )
        ]
        args.strings.parent.mkdir(parents=True, exist_ok=True)
        args.strings.write_text(json.dumps(strings, indent=2) + "\n")
    if args.frames:
        frames = [asdict(item) for item in iter_gree_frames(data, manifest)]
        args.frames.parent.mkdir(parents=True, exist_ok=True)
        args.frames.write_text(json.dumps(frames, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
