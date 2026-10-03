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

# Exact checksum-valid frames transcribed from the October 3, 2026
# logic-analyzer screenshots supplied alongside maxim-smirnov/gree-wired-proto.
# The composite capture explicitly annotates an approximately 800 ms idle pause
# after the FF 40 frame before the 00 FF frame is sent.
XK19_SCREENSHOT_STATUS = (
    "7E 7E FF 40 11 16 "
    "0C 30 83 80 77 00 04 00 00 04 00 A8 00 80 00 00 00 00 00 00 23 FB"
)

XK19_SCREENSHOT_POLL = (
    "7E 7E 00 FF 11 0E "
    "00 00 02 01 82 86 6E 52 00 80 00 00 20 7B"
)

XK19_SCREENSHOT_CONTROLLER = (
    "7E 7E FF 00 11 15 "
    "0C 30 83 01 13 7D 03 10 E0 E0 08 00 0A 00 00 00 00 00 00 00 3A"
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

    def test_xk19_screenshot_wakeup_sequence_is_retained_exactly(self):
        analysis = trace.analyze_text(
            XK19_SCREENSHOT_STATUS
            + "\n"
            + XK19_SCREENSHOT_POLL
            + "\n"
            + XK19_SCREENSHOT_CONTROLLER
        )
        self.assertEqual(len(analysis.frames), 3)
        self.assertTrue(all(frame.xor_valid for frame in analysis.frames))
        self.assertEqual(
            [frame.total_length for frame in analysis.frames],
            [28, 20, 27],
        )
        self.assertEqual(
            [frame.reference_layout for frame in analysis.frames],
            [
                "xk19_status_ff40_16",
                "legacy_poll_00_ff_0e",
                "xk19_controller_state_ff00_15",
            ],
        )
        self.assertEqual(
            [frame.signature for frame in analysis.frames],
            ["0C 30 83", None, "0C 30 83"],
        )
        session = trace.session_summary(analysis)
        self.assertEqual(
            session["pattern"],
            "legacy_indoor_first_then_controller_reply",
        )
        self.assertEqual(session["statuses_before_first_controller"], 1)
        self.assertEqual(session["polls_before_first_controller"], 1)
        self.assertFalse(session["expanded_status_after_controller"])

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

    def test_session_summary_detects_indoor_first_registration_sequence(self):
        text = (
            GKH_PRE_STATUS
            + "\n7E 7E 00 FF 11 0E 00 00 02 00 3C 3C 00 F6 00 00 00 00 00 14"
            + "\n"
            + GKH_XK76_CONTROLLER
            + "\n"
            + GKH_REGISTERED_STATUS
        )
        analysis = trace.analyze_text(text)
        session = trace.session_summary(analysis)
        self.assertEqual(
            session["pattern"],
            "legacy_indoor_first_then_controller_registration",
        )
        self.assertEqual(session["legacy_polls"], 1)
        self.assertEqual(session["pre_registration_status_frames"], 1)
        self.assertEqual(session["controller_state_frames"], 1)
        self.assertEqual(session["expanded_status_frames"], 1)
        self.assertEqual(session["polls_before_first_controller"], 1)
        self.assertEqual(session["statuses_before_first_controller"], 1)
        self.assertTrue(session["expanded_status_after_controller"])

    def test_session_summary_detects_indoor_first_without_controller(self):
        text = (
            GKH_PRE_STATUS
            + "\n7E 7E 00 FF 11 0E 00 00 02 00 3C 3C 00 F6 00 00 00 00 00 14"
        )
        session = trace.session_summary(trace.analyze_text(text))
        self.assertEqual(
            session["pattern"],
            "legacy_indoor_first_no_controller_reply",
        )
        self.assertEqual(session["controller_state_frames"], 0)
        self.assertFalse(session["expanded_status_after_controller"])

    def test_session_summary_does_not_promote_controller_only_capture(self):
        session = trace.session_summary(trace.analyze_text(GKH_XK76_CONTROLLER))
        self.assertEqual(
            session["pattern"],
            "controller_frame_without_observed_indoor_startup",
        )

    def test_esphome_log_tx_direction_and_timestamp_are_preserved(self):
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
        self.assertEqual(analysis.frames[0].direction, "tx")

    def test_esphome_rx_line_is_not_confused_with_local_tx(self):
        line = (
            "[02:25:44.000][I][gree_wired_rs485:999]: RX raw burst: "
            + XK19_CONTROLLER
        )
        analysis = trace.analyze_text(line)
        self.assertEqual(len(analysis.frames), 1)
        self.assertEqual(analysis.frames[0].direction, "rx")

    def test_unlabeled_sniffer_frame_remains_observed(self):
        analysis = trace.analyze_text(XK19_CONTROLLER)
        self.assertEqual(len(analysis.frames), 1)
        self.assertEqual(analysis.frames[0].direction, "observed")

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
        self.assertEqual(analysis.frames[0].direction, "observed")

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
