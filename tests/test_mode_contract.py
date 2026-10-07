"""Behavioral regression tests live in test_protocol_frame.cpp.

This harness compiles and executes them so tests validate request ownership and
packet preservation rather than accepting implementation-source strings.
"""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class ModeContractTests(unittest.TestCase):
    def test_request_lifecycle_and_packet_preservation(self):
        binary = pathlib.Path(tempfile.gettempdir()) / "sinclair_protocol_behavior"
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-pedantic",
                        "tests/test_protocol_frame.cpp", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)

if __name__ == "__main__":
    unittest.main()
