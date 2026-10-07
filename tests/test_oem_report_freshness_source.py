import pathlib
import unittest


class OemReportFreshnessSourceTest(unittest.TestCase):
    def setUp(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        self.header = (
            root / "components/gree_oem_report_sensors/gree_oem_report_sensors.h"
        ).read_text()

    def test_outdoor_report_uses_generation_freshness(self):
        self.assertIn(
            "get_retained_payload_generation(0x35)",
            self.header,
        )
        self.assertIn(
            "generation == this->last_outdoor_generation_",
            self.header,
        )
        self.assertNotIn(
            "*payload == this->last_outdoor_decoded_payload_",
            self.header,
        )
        self.assertIn(
            "fields.ambient_temperature_candidate_c, true",
            self.header,
        )
        self.assertIn(
            "fields.compressor_frequency_raw, true",
            self.header,
        )

    def test_raw_payload_uses_generation_not_payload_equality(self):
        self.assertIn(
            "uint32_t &last_generation",
            self.header,
        )
        self.assertIn(
            "generation == last_generation",
            self.header,
        )
        self.assertIn(
            "this->last_0x35_generation_",
            self.header,
        )
        self.assertNotIn(
            "if (payload == nullptr || *payload == last) return;",
            self.header,
        )

    def test_stabilized_temperature_refresh_preserves_accepted_value(self):
        self.assertIn(
            "else if (force && sensor->has_state())",
            self.header,
        )
        self.assertIn(
            "sensor->publish_state(sensor->state);",
            self.header,
        )


if __name__ == "__main__":
    unittest.main()
