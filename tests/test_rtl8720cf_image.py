import hashlib
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "firmware_research"))

from rtl8720cf_image import (  # noqa: E402
    FirmwareFormatError,
    file_offset_to_vma,
    iter_gree_frames,
    iter_printable_strings,
    parse_firmware,
)


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value)


def write_subimage(
    data: bytearray,
    *,
    offset: int,
    segment_size: int,
    next_image: int,
    image_type: int,
    section_size: int,
    section_type: int,
    image_length: int,
    section_base: int,
    entry_address: int,
    payload: bytes,
) -> None:
    put_u32(data, offset, segment_size)
    put_u32(data, offset + 4, next_image)
    data[offset + 8] = image_type
    section = offset + 0xC0
    put_u32(data, section, section_size)
    put_u32(data, section + 4, 0xFFFFFFFF)
    data[section + 8] = section_type
    entry = section + 0x60
    put_u32(data, entry, image_length)
    put_u32(data, entry + 4, section_base)
    put_u32(data, entry + 8, entry_address)
    image = entry + 0x20
    data[image : image + len(payload)] = payload


def make_image() -> bytes:
    data = bytearray(b"\xFF" * 0x13000)
    first_payload = b"bootstrap"
    write_subimage(
        data,
        offset=0xE0,
        segment_size=0x4C20,
        next_image=0x7F20,
        image_type=0x02,
        section_size=0x4B48,
        section_type=0x82,
        image_length=len(first_payload),
        section_base=0x10000480,
        entry_address=0x10000481,
        payload=first_payload,
    )
    frame = bytes((0x7E, 0x7E, 0x03, 0x0D, 0x00, 0x10))
    app_payload = b"uart energy compressor frequency EXV\x00" + frame
    write_subimage(
        data,
        offset=0x8000,
        segment_size=0xA000,
        next_image=0xFFFFFFFF,
        image_type=0x08,
        section_size=0x9F20,
        section_type=0x85,
        image_length=len(app_payload),
        section_base=0x9B000140,
        entry_address=0x9B000141,
        payload=app_payload,
    )
    return bytes(data)


class RTL8720CFImageTests(unittest.TestCase):
    def test_parse_known_layout_and_map_file_offset(self):
        data = make_image()
        manifest = parse_firmware(data, "fixture.bin")
        self.assertEqual(manifest.size, len(data))
        self.assertEqual(manifest.sha256, hashlib.sha256(data).hexdigest())
        self.assertEqual(len(manifest.sub_images), 2)

        bootstrap = manifest.sub_images[0]
        self.assertEqual(bootstrap.offset, 0xE0)
        self.assertEqual(bootstrap.segment_size, 0x4C20)
        self.assertEqual(bootstrap.next_image, 0x7F20)
        self.assertEqual(bootstrap.sections[0].entry.section_base, 0x10000480)

        application = manifest.sub_images[1]
        self.assertEqual(application.offset, 0x8000)
        section = application.sections[0]
        self.assertEqual(section.image_offset, 0x8140)
        self.assertEqual(section.entry.section_base, 0x9B000140)
        self.assertEqual(file_offset_to_vma(manifest, 0x8145), 0x9B000145)
        self.assertIsNone(file_offset_to_vma(manifest, 0x7000))

    def test_strings_and_valid_gree_frames(self):
        data = make_image()
        manifest = parse_firmware(data)
        strings = [item.text for item in iter_printable_strings(data, manifest)]
        self.assertIn("uart energy compressor frequency EXV", strings)

        frames = list(iter_gree_frames(data, manifest))
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].command, 0x0D)
        self.assertEqual(frames[0].declared_length, 3)
        self.assertEqual(frames[0].vma, 0x9B000165)

    def test_bad_checksum_is_not_a_frame(self):
        data = bytearray(make_image())
        data[0x8140 + len(b"uart energy compressor frequency EXV\x00") + 5] ^= 1
        manifest = parse_firmware(bytes(data))
        self.assertEqual(list(iter_gree_frames(bytes(data), manifest)), [])

    def test_rejects_truncated_image(self):
        with self.assertRaises(FirmwareFormatError):
            parse_firmware(b"\x00" * 32)


if __name__ == "__main__":
    unittest.main()
