import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


class PayloadEvolutionTest(unittest.TestCase):
    def test_cpp_behavior(self):
        root = Path(__file__).resolve().parents[1]
        source = textwrap.dedent(r"""
            #include <cassert>
            #include <cstdint>
            #include <vector>
            #include "components/gree_oem_boot_probe/payload_evolution.h"
            using esphome::gree_oem_probe::PayloadEvolution;
            using esphome::gree_oem_probe::payload_hex;
            int main() {
              PayloadEvolution profile;
              const std::vector<uint8_t> a{0x04, 0x20, 0x40};
              const std::vector<uint8_t> b{0x04, 0x28, 0x3F};
              assert(profile.observe(a) == "baseline");
              assert(profile.observe(a) == "none");
              assert(profile.observe(b) == "1:20>28,2:40>3F");
              assert(profile.samples() == 3);
              assert(profile.minimum(1) == 0x20);
              assert(profile.maximum(1) == 0x28);
              assert(profile.xor_mask(1) == 0x08);
              assert(profile.change_count(1) == 1);
              assert(payload_hex(b) == "04.28.3F");
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
