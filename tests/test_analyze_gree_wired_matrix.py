from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import analyze_gree_wired_matrix as matrix  # noqa: E402


def capture(profile: str, *, tx: bool = False, seconds: int = 65) -> str:
    tx_line = (
        "[02:25:00.500][I][gree_wired_rs485]: "
        "TX legacy GKH/XK76 registration 1/4: 7E 7E FF 00\n"
        if tx else ""
    )
    return (
        f"[02:25:00.000][I][gree_wired_rs485]: Passive UART profile scan enabled; "
        f"RS485 transmitter remains disabled; boot_profile={profile}\n"
        f"[02:25:00.001][I][gree_wired_rs485]: SCAN listening profile={profile} phase=boot-staged\n"
        f"{tx_line}"
        f"[02:25:01.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE profile={profile} "
        "bytes=0 uart_window=0 valid=0 rx_edges_window=0 rx_edges_total=0 "
        "edge_cadence_samples=0 edge_min_gap_us=0 edge_max_gap_us=0 edge_last_gap_us=0\n"
        f"[02:26:{seconds - 60 + 1:02d}.000][I][gree_wired_rs485]: HEALTH mode=PASSIVE "
        f"profile={profile} bytes=0 uart_window=0 valid=0 rx_edges_window=0 "
        "rx_edges_total=0 edge_cadence_samples=0 edge_min_gap_us=0 "
        "edge_max_gap_us=0 edge_last_gap_us=0\n"
    )


class GreeWiredMatrixTests(unittest.TestCase):
    def write(self, root: Path, name: str, text: str) -> Path:
        path = root / name
        path.write_text(text)
        return path

    def test_complete_matrix_requires_each_profile_once_and_no_tx(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            paths = [
                self.write(root, f"{profile}.log", capture(profile))
                for profile in matrix.EXPECTED_PROFILES
            ]
            rows = matrix.analyze_paths(paths)
            cov = matrix.coverage(rows)
            self.assertTrue(cov["complete_passive_matrix"])
            self.assertEqual(cov["missing_profiles"], [])
            self.assertEqual(cov["duplicate_profiles"], [])
            self.assertEqual(cov["captures_with_tx"], [])

    def test_duplicate_missing_unknown_and_tx_are_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            paths = [
                self.write(root, "a.log", capture("1200-8N1")),
                self.write(root, "b.log", capture("1200-8N1", tx=True)),
                self.write(
                    root,
                    "unknown.log",
                    "[02:25:01.000][I][gree_wired_rs485]: "
                    "HEALTH mode=PASSIVE profile=1200-8N1 bytes=0 uart_window=0 "
                    "valid=0 rx_edges_window=0 rx_edges_total=0 "
                    "edge_cadence_samples=0 edge_min_gap_us=0 "
                    "edge_max_gap_us=0 edge_last_gap_us=0\n",
                ),
            ]
            cov = matrix.coverage(matrix.analyze_paths(paths))
            self.assertFalse(cov["complete_passive_matrix"])
            self.assertIn("1200-8N1", cov["duplicate_profiles"])
            self.assertIn("1200-8E1", cov["missing_profiles"])
            self.assertEqual(len(cov["unknown_profile_captures"]), 1)
            self.assertEqual(len(cov["captures_with_tx"]), 1)


if __name__ == "__main__":
    unittest.main()
