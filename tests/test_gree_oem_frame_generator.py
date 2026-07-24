from __future__ import annotations

import unittest

from tools.gree_oem_frame_generator import (
    QUERIES,
    RTL_REPORT_QUERY_RESERVED_INDEX,
    build_extended_query,
    build_full_mac_report,
    checksum,
    parse_mac,
    validate_extended_query,
    validate_frame,
)


class ExtendedQueryTests(unittest.TestCase):
    def test_audited_query_layout_and_checksums(self) -> None:
        expected = {
            "status": (0x00, 0x00, 0x31, 0x94),
            "fault_combined": (0x01, 0x00, 0x33, 0x95),
            "fault_indoor": (0x02, 0x00, 0x34, 0x96),
            "fault_outdoor": (0x04, 0x00, 0x35, 0x98),
            "service_page_42": (0x00, 0x01, 0x42, 0x95),
            "service_page_41": (0x00, 0x02, 0x41, 0x96),
            "energy_month": (0x00, 0x04, 0x40, 0x98),
        }
        for name, (primary, secondary, response, frame_checksum) in expected.items():
            with self.subTest(name=name):
                query = QUERIES[name]
                frame = build_extended_query(query)
                self.assertEqual(len(frame), 29)
                self.assertEqual(frame[:4], [0x7E, 0x7E, 0x1A, 0x03])
                self.assertEqual(frame[4], primary)
                self.assertEqual(frame[14], secondary)
                self.assertEqual(frame[26], 0x01)
                self.assertEqual(frame[RTL_REPORT_QUERY_RESERVED_INDEX], 0x00)
                self.assertEqual(frame[-1], frame_checksum)
                self.assertEqual(query.expected_response, response)
                validate_extended_query(frame)

    def test_old_28_byte_shape_is_rejected(self) -> None:
        frame = build_extended_query(QUERIES["fault_outdoor"])
        del frame[RTL_REPORT_QUERY_RESERVED_INDEX]
        frame[2] = 0x19
        frame[-1] = checksum(frame[:-1])
        with self.assertRaisesRegex(ValueError, "29 bytes"):
            validate_extended_query(frame)

    def test_module_state_and_state_flags_are_in_checksum(self) -> None:
        frame = build_extended_query(
            QUERIES["status"], module_state=0x7F, state_flags=0x80
        )
        self.assertEqual(frame[4], 0x80)
        self.assertEqual(frame[26], 0x7F)
        self.assertEqual(frame[27], 0x00)
        self.assertEqual(frame[-1], checksum(frame[:-1]))

    def test_invalid_query_inputs_fail_closed(self) -> None:
        with self.assertRaises(ValueError):
            build_extended_query(QUERIES["status"], module_state=0x100)
        with self.assertRaises(ValueError):
            build_extended_query(QUERIES["status"], state_flags=0x20)


class MacReportTests(unittest.TestCase):
    def test_full_mac_report(self) -> None:
        mac = parse_mac("02:11:22:33:44:55")
        frame = build_full_mac_report(mac)
        self.assertEqual(len(frame), 15)
        self.assertEqual(frame[:4], [0x7E, 0x7E, 0x0C, 0x04])
        self.assertEqual(frame[8:14], list(mac))
        validate_frame(frame)

    def test_invalid_mac(self) -> None:
        with self.assertRaises(ValueError):
            parse_mac("not-a-mac")
        with self.assertRaises(ValueError):
            build_full_mac_report(b"short")


if __name__ == "__main__":
    unittest.main()
