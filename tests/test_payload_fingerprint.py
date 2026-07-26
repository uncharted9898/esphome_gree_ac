import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


class PayloadFingerprintTest(unittest.TestCase):
    def test_cpp_behavior(self):
        root = Path(__file__).resolve().parents[1]
        source = textwrap.dedent(r"""
            #include <cassert>
            #include <cstdint>
            #include <vector>
            #include "components/gree_oem_boot_probe/payload_fingerprint.h"
            using esphome::gree_oem_probe::payload_changed_indices;
            using esphome::gree_oem_probe::payload_fnv1a;
            int main() {
              const std::vector<uint8_t> baseline{0x04, 0x00, 0x40, 0x00};
              const std::vector<uint8_t> same{0x04, 0x00, 0x40, 0x00};
              const std::vector<uint8_t> changed{0x04, 0x00, 0x41, 0x00};
              const std::vector<uint8_t> longer{0x04, 0x00, 0x40, 0x00, 0x01};
              assert(payload_fnv1a(baseline) == payload_fnv1a(same));
              assert(payload_fnv1a(baseline) != payload_fnv1a(changed));
              assert(payload_changed_indices(baseline, same) == "none");
              assert(payload_changed_indices(baseline, changed) == "2");
              assert(payload_changed_indices(baseline, longer) == "size:4->5");
              return 0;
            }
        """)
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "test.cpp"
            exe = Path(tmp) / "test"
            src.write_text(source)
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I", str(root), str(src), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
