from pathlib import Path
import csv
import io
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import analyze_gree_wired_trace as trace  # noqa: E402


XK19_CONTROLLER = (
    "7E 7E FF 00 11 15 "
    "0C 30 83 01 14 7E 22 10 E0 E0 08 00 02 00 00 00 00 00 00 00 17"
)

GKH_XK76_CONTROLLER = (
    "7E 7E FF 00 11 22 "
    "09 30 83 11 1B 00 00 10 E0 E0 08 00 28 00 00 00 00 00 00 00 "
    "21 00 00 00 00 00 05 00 00 00 00 00 30 58"
)

GKH_PRE_STATUS = (
    "7E 7E FF 40 11 17 "
    "09 30 83 7F 70 0E 00 00 00 01 00 00 00 00 00 00 00 00 00 00 "
    "32 02 33"
)

GKH_REGISTERED_STATUS = (
    "7E 7E FF 40 11 29 "
    "09 30 83 7F 70 0E 00 00 00 01 00 00 00 00 00 00 00 00 00 00 "
    "32 02 11 1B 00 00 04 28 00 00 00 00 00 00 00 00 00 00 00 20 0B"
)


class GreeWiredTraceAnalyzerTests(unittest.TestCase):
    def test_xk19_controller_reference_is_distinct_from_gkh_xk76(self):
        analysis = trace.analyze_text(XK19_CONTROLLER)
        self.assertEqual(len(analysis.frames), 1)
        frame = analysis.frames[0]
        self.assertTrue(frame.xor_valid)
        self.assertEqual(frame.route, "FF->00")
        self.assertEqual(frame.body_length, 0x15)
        self.assertEqual(frame.reference_layout, "xk19_controller_state_ff00_15")
        self.assertEqual(frame.signature, "0C 30 83")

    def test_gkh_xk76_registration_frame_is_labeled_by_provenance(self):
        analysis = trace.analyze_text(GKH_XK76_CONTROLLER)
        self.assertEqual(len(analysis.frames), 1)
        frame = analysis.frames[0]
        self.assertTrue(frame.xor_valid)
        self.assertEqual(frame.total_length, 40)
        self.assertEqual(frame.body_length, 0x22)
        self.assertEqual(
            frame.reference_layout,
            "gkh_xk76_controller_state_ff00_22",
        )
        self.assertEqual(frame.signature, "09 30 83")

    def test_gkh_status_expansion_is_detected_without_calling_it_xe71(self):
        analysis = trace.analyze_text(
            GKH_PRE_STATUS + "\n" + GKH_REGISTERED_STATUS
        )
        self.assertEqual(
            [frame.reference_layout for frame in analysis.frames],
            [
                "gkh_pre_registration_status_ff40_17",
                "gkh_registered_status_ff40_29",
            ],
        )
        self.assertTrue(all(frame.xor_valid for frame in analysis.frames))

    def test_esphome_log_timestamp_is_preserved(self):
        line = (
            "[02:25:43.543][I][gree_wired_rs485:854]: TX legacy: "
            + GKH_XK76_CONTROLLER
        )
        analysis = trace.analyze_text(line)
        self.assertEqual(len(analysis.frames), 1)
        self.assertAlmostEqual(
            analysis.frames[0].timestamp_s,
            2 * 3600 + 25 * 60 + 43.543,
            places=6,
        )
        self.assertEqual(analysis.frames[0].source_line, 1)

    def test_saleae_byte_csv_is_reassembled_by_declared_length(self):
        values = [int(token, 16) for token in XK19_CONTROLLER.split()]
        output = io.StringIO()
        writer = csv.writer(output)
        writer.writerow(["Time [s]", "Value"])
        for index, value in enumerate(values):
            writer.writerow([f"{index / 1200.0:.9f}", f"0x{value:02X}"])

        analysis = trace.analyze_saleae_csv(output.getvalue())
        self.assertEqual(len(analysis.frames), 1)
        self.assertTrue(analysis.frames[0].xor_valid)
        self.assertEqual(
            analysis.frames[0].reference_layout,
            "xk19_controller_state_ff00_15",
        )
        self.assertEqual(analysis.frames[0].timestamp_s, 0.0)

    def test_bad_xor_is_retained_as_evidence(self):
        values = [int(token, 16) for token in GKH_PRE_STATUS.split()]
        values[-1] ^= 0x01
        analysis = trace.analyze_text(" ".join(f"{value:02X}" for value in values))
        self.assertEqual(len(analysis.frames), 1)
        self.assertFalse(analysis.frames[0].xor_valid)

    def test_truncated_candidate_is_reported(self):
        analysis = trace.analyze_text("7E 7E FF 40 11 29 09 30")
        self.assertEqual(analysis.frames, [])
        self.assertEqual(analysis.truncated_candidates, 1)


if __name__ == "__main__":
    unittest.main()
