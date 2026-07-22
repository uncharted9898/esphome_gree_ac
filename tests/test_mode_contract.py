"""Source-level safety contracts that do not require ESPHome headers or hardware."""
from pathlib import Path
import unittest
ROOT = Path(__file__).parents[1]
CNT = (ROOT / "components/sinclair_ac/esppac_cnt.cpp").read_text()
class ModeContractTests(unittest.TestCase):
    def test_receive_only_has_no_uart_write_path(self):
        self.assertIn("void SinclairACCNT::send_packet()\n{\n    if (this->is_receive_only()) return;", CNT)
        self.assertEqual(CNT.count("write_array(packet)"), 1)
    def test_poll_only_clears_updates_before_packet_build(self):
        self.assertIn("if (this->is_poll_only()) this->update_ = ACUpdate::NoUpdate;", CNT)
        self.assertIn("packet[protocol::SET_NOCHANGE_BYTE] |= protocol::SET_NOCHANGE_MASK;", CNT)
    def test_control_callbacks_are_guarded(self):
        self.assertGreaterEqual(CNT.count("if (!this->can_control()) return;"), 8)
    def test_invalid_or_unknown_do_not_release_response_guard(self):
        self.assertIn("if (known && this->wait_response_) this->wait_response_ = false;", CNT)
        self.assertNotIn("this->wait_response_ = false;\n        /* log", CNT)
if __name__ == "__main__": unittest.main()
