#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

cleanup_validation_files() {
  if [ "${KEEP_VALIDATION_YAML:-0}" != "1" ]; then
    rm -f examples/.validation-*.yaml
  fi
  find . -type d -name __pycache__ -prune -exec rm -rf {} +
  find . -type f \( -name '*.pyc' -o -name '*.pyo' \) -delete
}
trap cleanup_validation_files EXIT

# The repository examples intentionally use a local secrets file. GitHub Actions
# runners are ephemeral and do not have a user secrets.yaml, so create harmless
# CI-only values when the standard CI=true environment is present. Local builds
# still require the user's real secrets file and are never overwritten.
if [ "${CI:-false}" = "true" ] && [ ! -f examples/secrets.yaml ]; then
  cat > examples/secrets.yaml <<'EOF'
wifi_ssid: ci-network
wifi_password: ci-password
wifi_ap_passwd: ci-ap-password
ota_password: ci-ota-password
gree_livo_api_key: AQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQE=
api_encryption_key: AgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgI=
gree_livo_web_username: ci-user
gree_livo_web_password: ci-password
EOF
fi

python3 -m py_compile \
  components/sinclair_ac/climate.py \
  components/gree_oem_boot_probe/__init__.py \
  components/gree_oem_report_sensors/__init__.py \
  components/gree_wired_rs485/__init__.py \
  tools/firmware_research/rtl8720cf_image.py \
  tools/firmware_research/analyze_rtl8720cf.py \
  tools/firmware_research/rtl8720cf_properties.py \
  tools/firmware_research/setup_rtl8720cf.py \
  tools/firmware_research/load_rtl8720cf_companion.py \
  tools/firmware_research/seed_rtl8720cf_functions.py \
  tools/firmware_research/audit_rtl8720cf_protocol.py \
  tools/firmware_research/audit_rtl8720cf_handshake.py \
  tools/gree_oem_frame_generator.py \
  tools/analyze_gree_wired_trace.py \
  tools/analyze_gree_wired_discovery.py \
  tools/analyze_gree_wired_matrix.py

python3 -m unittest discover -s tests -p 'test_*.py' -v

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_protocol_frame.cpp -o /tmp/test_protocol_frame
/tmp/test_protocol_frame

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_oem_report_decoder.cpp -o /tmp/test_oem_report_decoder
/tmp/test_oem_report_decoder

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_rtl_report_query.cpp -o /tmp/test_rtl_report_query
/tmp/test_rtl_report_query

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_rtl044_handshake.cpp -o /tmp/test_rtl044_handshake
/tmp/test_rtl044_handshake

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_wired_protocol.cpp -o /tmp/test_wired_protocol
/tmp/test_wired_protocol

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_oem_wired_probe.cpp -o /tmp/test_oem_wired_probe
/tmp/test_oem_wired_probe

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_line_activity.cpp -o /tmp/test_line_activity
/tmp/test_line_activity

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_controller_registration.cpp -o /tmp/test_controller_registration
/tmp/test_controller_registration

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_registration_rx_window.cpp -o /tmp/test_registration_rx_window
/tmp/test_registration_rx_window

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_wired_controller_state.cpp -o /tmp/test_wired_controller_state
/tmp/test_wired_controller_state

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_wired_status.cpp -o /tmp/test_wired_status
/tmp/test_wired_status

if grep -RInE 'github://piotrva/esphome_gree_ac$|@main' examples; then
  echo 'Examples contain an unpinned or obsolete external component source' >&2
  exit 1
fi

if ! command -v esphome >/dev/null 2>&1; then
  echo 'esphome is required for YAML validation' >&2
  exit 1
fi
esphome --version | grep -F '2026.9.1'

python3 - <<'PY'
from pathlib import Path
import re

component_path = Path('components').resolve()
sources = (
    Path('examples/gree-livo-oem-boot-probe.yaml'),
    Path('examples/gree-livo-gen3-refined-discovery.yaml'),
    Path('examples/gree-livo-gen3-full-power-discovery.yaml'),
    Path('examples/gree-vireo-xiao-rs485-listen-only.yaml'),
)
pattern = re.compile(
    r'(?m)^  - source:\n'
    r'      type: git\n'
    r'      url: https://github\.com/uncharted9898/esphome_gree_ac\n'
    r'      ref: [^\n]+\n'
)
replacement = (
    '  - source:\n'
    '      type: local\n'
    f'      path: {component_path}\n'
)
for source in sources:
    target = source.with_name(f'.validation-{source.name}')
    localized, count = pattern.subn(replacement, source.read_text(), count=1)
    if count != 1:
        raise SystemExit(f'{source}: expected one repository source, found {count}')
    target.write_text(localized)
    print(target)

vireo_base = '.validation-gree-vireo-xiao-rs485-listen-only.yaml'
wrappers = {
    '.validation-gree-vireo-passive-profile-scan.yaml': (
        'packages:\n'
        f'  base: !include {vireo_base}\n'
        '  scan: !include ../packages/gree-vireo-xiao-rs485-passive-profile-scan.yaml\n'
    ),
    '.validation-gree-vireo-legacy-gkh-xk76-probe.yaml': (
        'packages:\n'
        f'  base: !include {vireo_base}\n'
        '  legacy: !include ../packages/gree-vireo-xiao-rs485-legacy-gkh-xk76-probe.yaml\n'
    ),
    '.validation-gree-vireo-oem-rtl-probe.yaml': (
        'packages:\n'
        f'  base: !include {vireo_base}\n'
        '  oem: !include ../packages/gree-vireo-xiao-rs485-oem-rtl-probe.yaml\n'
    ),
    '.validation-gree-vireo-passive-profile-9600-8e1.yaml': (
        'substitutions:\n'
        '  gree_wired_scan_start_profile: "9600-8E1"\n'
        'packages:\n'
        f'  base: !include {vireo_base}\n'
        '  scan: !include ../packages/gree-vireo-xiao-rs485-passive-profile-scan.yaml\n'
    ),
    '.validation-gree-vireo-invalid-active-without-legacy.yaml': (
        'packages:\n'
        f'  base: !include {vireo_base}\n'
        'gree_wired_rs485:\n'
        '  id: gree_com_manual\n'
        '  active_probe: true\n'
    ),
}
for name, content in wrappers.items():
    target = Path('examples') / name
    target.write_text(content)
    print(target)
PY

for example in \
  examples/gree-livo-gen3-receive-only.yaml \
  examples/gree-livo-gen3-poll-only.yaml \
  examples/gree-livo-gen3-control.yaml \
  examples/.validation-gree-livo-gen3-refined-discovery.yaml \
  examples/.validation-gree-livo-gen3-full-power-discovery.yaml \
  examples/.validation-gree-livo-oem-boot-probe.yaml \
  examples/.validation-gree-vireo-xiao-rs485-listen-only.yaml \
  examples/.validation-gree-vireo-passive-profile-scan.yaml \
  examples/.validation-gree-vireo-passive-profile-9600-8e1.yaml \
  examples/.validation-gree-vireo-legacy-gkh-xk76-probe.yaml \
  examples/.validation-gree-vireo-oem-rtl-probe.yaml; do
  esphome config "$example"
done

invalid_probe_log="/tmp/gree-vireo-invalid-active-probe.log"
if esphome config examples/.validation-gree-vireo-invalid-active-without-legacy.yaml \
    >"$invalid_probe_log" 2>&1; then
  echo 'active_probe without legacy_gkh_xk76_probe unexpectedly validated' >&2
  cat "$invalid_probe_log" >&2
  exit 1
fi
if ! grep -F 'legacy_gkh_xk76_probe' "$invalid_probe_log" >/dev/null; then
  echo 'invalid active probe failed for an unexpected reason' >&2
  cat "$invalid_probe_log" >&2
  exit 1
fi

# Compile the localized OEM boot-probe path as well. This is the ESP-IDF/C3
# configuration that exercises the recovered telemetry query components and
# catches integration issues that config validation alone cannot see.
esphome compile examples/.validation-gree-livo-oem-boot-probe.yaml

# Compile the deployment target for the pre-soldered Seeed XIAO ESP32-C3 +
# RS485 expansion board. This verifies the passive 1200-8N1 baseline.
esphome compile examples/.validation-gree-vireo-xiao-rs485-listen-only.yaml

# Compile the bounded controller-first OEM experiment as well. The YAML starts
# from the same passive image and changes the live UART to 4800-8E1 only at
# runtime after the configured delay.
esphome compile examples/.validation-gree-vireo-oem-rtl-probe.yaml
