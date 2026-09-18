import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


class TemperatureStabilizerTest(unittest.TestCase):
    def test_cpp_behavior(self):
        root = Path(__file__).resolve().parents[1]
        source = textwrap.dedent(r"""
            #include <cassert>
            #include "components/sinclair_ac/temperature_stabilizer.h"
            using esphome::sinclair_ac::TemperatureStabilizer;
            int main() {
              TemperatureStabilizer s;
              float out = 0.0f;
              assert(s.process(22.0f, 100, true, 8000, 2.0f, out) && out == 22.0f);
              assert(!s.process(21.0f, 200, true, 8000, 2.0f, out));
              assert(!s.process(22.0f, 300, true, 8000, 2.0f, out));
              assert(!s.process(21.0f, 1000, true, 8000, 2.0f, out));
              assert(!s.process(21.0f, 8999, true, 8000, 2.0f, out));
              assert(s.process(21.0f, 9000, true, 8000, 2.0f, out) && out == 21.0f);
              assert(s.process(23.0f, 9100, true, 8000, 2.0f, out) && out == 23.0f);
              assert(s.process(22.0f, 9200, false, 8000, 2.0f, out) && out == 22.0f);
              s.reset();
              assert(s.process(20.0f, 0xFFFFFF00u, true, 500, 2.0f, out));
              assert(!s.process(21.0f, 0xFFFFFF10u, true, 500, 2.0f, out));
              assert(s.process(21.0f, 0x00000110u, true, 500, 2.0f, out));
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
