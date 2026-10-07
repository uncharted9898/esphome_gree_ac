from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import analyze_gree_wired_discovery as discovery  # noqa: E402


class GreeWiredDiscoveryAnalyzerTests(unittest.TestCase):
    def test_empty_or_unrelated_log_is_not_called_electrically_silent(self):
        analysis = discovery.analyze_log("")
        self.assertEqual(discovery.conclusion(analysis), "insufficient_evidence")

        unrelated = discovery.analyze_log("INFO WiFi connected\nINFO API ready\n")
        self.assertEqual(discovery.conclusion(unrelated), "insufficient_evidence")

    def test_boot_profile_and_capture_duration_are_retained(self):
        text = """
[02:25:00.000][I][gree_wired_rs485]: Passive UART profile scan enabled; RS485 transmitter remains disabled; boot_profile=9600-8E1
[02:25:00.001][I][gree_wired_rs485]: SCAN listening profile=9600-8E1 phase=boot-staged
[02:25:01.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=9600-8E1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
[02:26:06.500][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(analysis.declared_boot_profile, "9600-8E1")
        self.assertFalse(analysis.boot_profile_conflict)
        self.assertAlmostEqual(analysis.capture_duration_s, 65.5)
        summary = discovery.summary_dict(analysis)
        self.assertEqual(summary["declared_boot_profile"], "9600-8E1")
        self.assertEqual(len(summary["boot_profiles"]), 2)

    def test_conflicting_boot_profile_evidence_is_flagged(self):
        text = """
[I][gree_wired_rs485]: Passive UART profile scan enabled; RS485 transmitter remains disabled; boot_profile=9600-8E1
[I][gree_wired_rs485]: SCAN listening profile=1200-8N1 phase=boot-staged
"""
        analysis = discovery.analyze_log(text)
        self.assertIsNone(analysis.declared_boot_profile)
        self.assertTrue(analysis.boot_profile_conflict)

    def test_clean_vireo_capture_is_reported_as_electrically_silent(self):
        text = """
[02:25:48.608][I][gree_wired_rs485:424]: HEALTH mode=PASSIVE dir=MANUAL profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
[02:25:53.608][I][gree_wired_rs485:380]: SCAN profile=1200-8N1 bytes=0 legacy_valid=0
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(discovery.conclusion(analysis), "electrically_silent")
        self.assertEqual(analysis.max_edges_total, 0)
        self.assertEqual(analysis.max_bytes_total, 0)
        self.assertEqual(discovery.profile_summary(analysis)[0]["profile"], "1200-8N1")

    def test_extended_zero_activity_journal_remains_electrically_silent(self):
        lines = []
        for second in range(0, 601, 10):
            minute, sec = divmod(second, 60)
            lines.append(
                f"[02:{25 + minute:02d}:{sec:02d}.000][I][gree_wired_rs485]: "
                "HEALTH mode=PASSIVE dir=MANUAL profile=1200-8N1 "
                "bytes=0 uart_window=0 valid=0 rx_edges_window=0 "
                "rx_edges_total=0 edge_cadence_samples=0 "
                "edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0"
            )
        analysis = discovery.analyze_log("\n".join(lines))
        self.assertGreater(len(analysis.health), 10)
        self.assertEqual(discovery.conclusion(analysis), "electrically_silent")
        self.assertEqual(discovery.summary_dict(analysis)["max_edges_total"], 0)
        summary = discovery.summary_dict(analysis)
        self.assertEqual(summary["max_bytes_total"], 0)
        self.assertTrue(summary["sustained_physical_silence"])
        self.assertEqual(
            summary["recommended_next_step"],
            "capture_or_emulate_xk76ca_indoor_side_before_profile_sweep",
        )

    def test_short_zero_activity_capture_requires_more_time(self):
        text = """
[02:25:00.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
[02:25:20.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
"""
        analysis = discovery.analyze_log(text)
        self.assertFalse(discovery.sustained_physical_silence(analysis))
        self.assertEqual(
            discovery.recommended_next_step(analysis),
            "extend_capture_to_60s_before_concluding_physical_silence",
        )

    def test_health_clock_reset_does_not_fake_long_silent_capture(self):
        text = """
[02:25:00.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
[02:25:20.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
[00:00:05.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
[00:00:15.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(analysis.capture_duration_s, 20.0)
        self.assertFalse(discovery.sustained_physical_silence(analysis))

    def test_physical_edges_route_to_profile_matrix(self):
        text = """
[02:25:00.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=12 rx_edges_total=12 edge_cadence_samples=10 edge_min_gap_us=833 edge_max_gap_us=3332 edge_last_gap_us=1666
[02:26:05.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=12 edge_cadence_samples=10 edge_min_gap_us=833 edge_max_gap_us=3332 edge_last_gap_us=1666
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(
            discovery.recommended_next_step(analysis),
            "run_cold_start_serial_profile_matrix",
        )

    def test_scan_only_byte_evidence_is_not_called_silent(self):
        text = """
[I][gree_wired_rs485]: SCAN profile=9600-8N1 bytes=7 legacy_valid=0
[I][gree_wired_rs485]: SCAN profile=1200-8N1 bytes=0 legacy_valid=0
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(
            discovery.conclusion(analysis),
            "uart_decode_candidates_without_legacy_validation",
        )
        self.assertEqual(analysis.profile_bytes_total, 7)
        self.assertEqual(discovery.summary_dict(analysis)["profile_bytes_total"], 7)

    def test_profile_bytes_are_not_promoted_without_valid_legacy_frame(self):
        text = """
[I][gree_wired_rs485]: SCAN profile=9600-8N1 bytes=17 legacy_valid=0
[I][gree_wired_rs485]: SCAN profile=1200-8N1 bytes=2 legacy_valid=0
[I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=9600-8N1 bytes=19 uart_window=17 valid=0 rx_edges_window=20 rx_edges_total=20 edge_cadence_samples=18 edge_min_gap_us=104 edge_max_gap_us=1040 edge_last_gap_us=208
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(
            discovery.conclusion(analysis),
            "uart_decode_candidates_without_legacy_validation",
        )
        profiles = discovery.profile_summary(analysis)
        self.assertEqual(profiles[0]["profile"], "9600-8N1")
        self.assertEqual(profiles[0]["bytes"], 17)
        self.assertEqual(analysis.max_valid_frames, 0)

    def test_gpio_edges_without_decoded_bytes_are_distinct(self):
        text = """
[I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=12 rx_edges_total=12 edge_cadence_samples=10 edge_min_gap_us=833 edge_max_gap_us=3332 edge_last_gap_us=1666
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(
            discovery.conclusion(analysis),
            "edge_activity_without_uart_decode",
        )
        hints = discovery.cadence_hints(analysis.min_edge_gap_us)
        self.assertTrue(any(item.baud == 1200 for item in hints))

    def test_valid_legacy_frame_is_stronger_than_raw_byte_count(self):
        text = """
[I][gree_wired_rs485]: SCAN profile=9600-8N1 bytes=100 legacy_valid=0
[I][gree_wired_rs485]: SCAN profile=1200-8N1 bytes=25 legacy_valid=1
[I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 bytes=125 uart_window=25 valid=1 rx_edges_window=50 rx_edges_total=50 edge_cadence_samples=40 edge_min_gap_us=833 edge_max_gap_us=5000 edge_last_gap_us=833
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(
            discovery.conclusion(analysis),
            "legacy_frame_evidence_present",
        )
        self.assertEqual(discovery.profile_summary(analysis)[0]["profile"], "1200-8N1")

    def test_tx_line_marks_capture_as_non_passive(self):
        text = """
[I][gree_wired_rs485]: TX legacy GKH/XK76 registration 1/4: 7E 7E FF 00
[I][gree_wired_rs485]: HEALTH mode=LEGACY_GKH_XK76 profile=1200-8N1 bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0
"""
        analysis = discovery.analyze_log(text)
        self.assertEqual(
            discovery.conclusion(analysis),
            "capture_contains_tx_evidence",
        )
        self.assertEqual(analysis.transmit_lines, [2])

    def test_trimmed_tx_completion_or_runtime_line_marks_capture_non_passive(self):
        for tx_line in (
            "TX complete registration 1/4 elapsed=334ms expected_wire=~333ms",
            "TX controller runtime response poll=4 reply=1 counter=0x21 direction=MANUAL_GPIO",
            "Controller registration TX flush was not confirmed",
        ):
            with self.subTest(tx_line=tx_line):
                text = (
                    f"[I][gree_wired_rs485]: {tx_line}\n"
                    "[I][gree_wired_rs485]: HEALTH mode=PASSIVE profile=1200-8N1 "
                    "bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 "
                    "edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 "
                    "edge_last_gap_us=0\n"
                )
                analysis = discovery.analyze_log(text)
                self.assertEqual(
                    discovery.conclusion(analysis),
                    "capture_contains_tx_evidence",
                )
                self.assertEqual(analysis.transmit_lines, [1])

    def test_cadence_hints_allow_integer_bit_multiples(self):
        hints = discovery.cadence_hints(1666)
        self.assertTrue(
            any(item.baud == 1200 and item.multiplier == 2 for item in hints)
        )
        self.assertFalse(any(item.relative_error > 0.12 for item in hints))


if __name__ == "__main__":
    unittest.main()
