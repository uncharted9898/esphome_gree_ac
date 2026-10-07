from __future__ import annotations

import unittest

from tools.gree_oem_frame_generator import (
    QUERIES,
    RTL_REPORT_QUERY_RESERVED_INDEX,
    RtlDateTimeContext,
    build_energy_flow_query,
    build_extended_query,
    build_full_mac_report,
    build_startup_sync,
    checksum,
    parse_mac,
    validate_energy_flow_query,
    validate_extended_query,
    validate_frame,
)


class ExtendedQueryTests(unittest.TestCase):
    def test_audited_query_layout_and_checksums(self) -> None:
        expected = {
            "status": (0x00, 0x00, 0x31, 0x1E),
            "fault_combined": (0x01, 0x00, 0x33, 0x1F),
            "fault_indoor": (0x02, 0x00, 0x34, 0x20),
            "fault_outdoor": (0x04, 0x00, 0x35, 0x22),
            "service_page_42": (0x00, 0x01, 0x42, 0x1F),
            "service_page_41": (0x00, 0x02, 0x41, 0x20),
            "energy_month": (0x00, 0x04, 0x40, 0x22),
        }
        for name, (primary, secondary, response, frame_checksum) in expected.items():
            with self.subTest(name=name):
                query = QUERIES[name]
                frame = build_extended_query(query)
                self.assertEqual(len(frame), 29)
                self.assertEqual(frame[:4], [0x7E, 0x7E, 0x1A, 0x03])
                self.assertEqual(frame[4], primary)
                self.assertEqual(frame[8:14], [0x00] * 6)
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

    def test_module_state_and_time_context_flags_are_in_checksum(self) -> None:
        first = build_extended_query(
            QUERIES["status"], module_state=0x7F, state_flags=0x80
        )
        self.assertEqual(first[4], 0x80)
        self.assertEqual(first[8:11], [0x17, 0x3B, 0x3B])
        self.assertEqual(first[11:14], [0x00, 0x00, 0x00])
        self.assertEqual(first[26], 0x7F)
        self.assertEqual(first[-1], checksum(first[:-1]))

        both = build_extended_query(
            QUERIES["status"], module_state=0x01, state_flags=0xC0
        )
        self.assertEqual(both[8:11], [0x17, 0x3B, 0x3B])
        self.assertEqual(both[11:14], [0x17, 0x3B, 0x3B])
        self.assertEqual(both[-1], checksum(both[:-1]))

    def test_context_bytes_must_match_state_flags(self) -> None:
        frame = build_extended_query(QUERIES["status"])
        frame[10] = 0x3B
        frame[-1] = checksum(frame[:-1])
        with self.assertRaisesRegex(ValueError, "first RTL time-context"):
            validate_extended_query(frame)

        frame = build_extended_query(QUERIES["status"], state_flags=0x40)
        frame[11:14] = [0x00, 0x00, 0x00]
        frame[-1] = checksum(frame[:-1])
        with self.assertRaisesRegex(ValueError, "second RTL time-context"):
            validate_extended_query(frame)

    def test_invalid_query_inputs_fail_closed(self) -> None:
        with self.assertRaises(ValueError):
            build_extended_query(QUERIES["status"], module_state=0x100)
        with self.assertRaises(ValueError):
            build_extended_query(QUERIES["status"], state_flags=0x20)


class EnergyFlowQueryTests(unittest.TestCase):
    def test_exact_zero_filled_command_09(self) -> None:
        frame = build_energy_flow_query()
        self.assertEqual(len(frame), 53)
        self.assertEqual(frame[:4], [0x7E, 0x7E, 0x32, 0x09])
        self.assertEqual(frame[4:-1], [0x00] * 48)
        self.assertEqual(frame[-1], 0x3B)
        validate_energy_flow_query(frame)

    def test_nonzero_payload_is_rejected_even_with_repaired_checksum(self) -> None:
        frame = build_energy_flow_query()
        frame[24] = 0x01
        frame[-1] = checksum(frame[:-1])
        with self.assertRaisesRegex(ValueError, "zero-filled"):
            validate_energy_flow_query(frame)


class StartupSyncTests(unittest.TestCase):
    def test_zero_time_connected_frame_matches_neutral_status_envelope(self) -> None:
        startup = build_startup_sync()
        status = build_extended_query(QUERIES["status"])
        self.assertEqual(startup, status)
        self.assertEqual(startup[15:21], [0x00] * 6)
        self.assertEqual(startup[26], 0x01)

    def test_calendar_packing_matches_firmware_scheduler(self) -> None:
        frame = build_startup_sync(
            date_time=RtlDateTimeContext(2026, 7, 25, 16, 42, 59)
        )
        self.assertEqual(frame[15:21], [0x7E, 0xA7, 25, 16, 42, 59])
        self.assertEqual(frame[-1], checksum(frame[:-1]))
        validate_extended_query(frame)

    def test_calendar_fields_fail_closed(self) -> None:
        with self.assertRaises(ValueError):
            build_startup_sync(date_time=RtlDateTimeContext(year=0x1000))
        with self.assertRaises(ValueError):
            build_startup_sync(date_time=RtlDateTimeContext(month=0x10))


class MacReportTests(unittest.TestCase):
    def test_full_mac_report(self) -> None:
        mac = parse_mac("02:11:22:33:44:55")
        frame = build_full_mac_report(mac)
        self.assertEqual(len(frame), 16)
        self.assertEqual(frame[:4], [0x7E, 0x7E, 0x0D, 0x04])
        self.assertEqual(frame[8:14], list(mac))
        self.assertEqual(frame[14], 0x00)
        self.assertEqual(frame[-1], checksum(frame[:-1]))
        validate_frame(frame)

    def test_invalid_mac(self) -> None:
        with self.assertRaises(ValueError):
            parse_mac("not-a-mac")
        with self.assertRaises(ValueError):
            build_full_mac_report(b"short")


if __name__ == "__main__":
    unittest.main()
