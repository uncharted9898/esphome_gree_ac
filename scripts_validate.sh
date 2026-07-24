#!/usr/bin/env bash
set -euo pipefail
python3 -m py_compile \
  components/sinclair_ac/climate.py \
  components/gree_oem_report_sensors/__init__.py \
  tools/firmware_research/rtl8720cf_image.py \
  tools/firmware_research/analyze_rtl8720cf.py
python3 -m unittest tests/test_rtl8720cf_image.py
c++ -std=c++17 -Wall -Wextra -pedantic tests/test_protocol_frame.cpp -o /tmp/test_protocol_frame
/tmp/test_protocol_frame
c++ -std=c++17 -Wall -Wextra -pedantic tests/test_oem_report_decoder.cpp -o /tmp/test_oem_report_decoder
/tmp/test_oem_report_decoder
! rg -n 'github://piotrva/esphome_gree_ac$|@main' examples
for example in examples/gree-livo-gen3-{receive-only,poll-only,control}.yaml; do
  esphome --version | rg '2026\.7\.1'
  esphome config "$example"
done
