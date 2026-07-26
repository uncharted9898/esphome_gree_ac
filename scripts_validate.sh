#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

ci_secrets_created=0
cleanup_validation_files() {
  if [ "${KEEP_VALIDATION_YAML:-0}" != "1" ]; then
    rm -f examples/.validation-*.yaml
  fi
  if [ "$ci_secrets_created" = "1" ]; then
    rm -f examples/secrets.yaml
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
gree_livo_api_key: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=
api_encryption_key: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=
gree_livo_web_username: ci-user
gree_livo_web_password: ci-password
EOF
  ci_secrets_created=1
fi

python3 -m py_compile \
  components/sinclair_ac/climate.py \
  components/gree_oem_boot_probe/__init__.py \
  components/gree_oem_report_sensors/__init__.py \
  tools/firmware_research/rtl8720cf_image.py \
  tools/firmware_research/analyze_rtl8720cf.py \
  tools/firmware_research/rtl8720cf_properties.py \
  tools/firmware_research/setup_rtl8720cf.py \
  tools/firmware_research/load_rtl8720cf_companion.py \
  tools/firmware_research/seed_rtl8720cf_functions.py \
  tools/firmware_research/audit_rtl8720cf_handshake.py \
  tools/gree_oem_frame_generator.py

python3 -m unittest \
  tests/test_rtl8720cf_image.py \
  tests/test_gree_oem_frame_generator.py

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

if grep -RInE 'github://piotrva/esphome_gree_ac$|@main' examples; then
  echo 'Examples contain an unpinned or obsolete external component source' >&2
  exit 1
fi

if ! command -v esphome >/dev/null 2>&1; then
  echo 'esphome is required for YAML validation' >&2
  exit 1
fi
esphome --version | grep -F '2026.7.1'

python3 - <<'PY'
from pathlib import Path
import re

component_path = Path('components').resolve()
sources = (
    Path('examples/gree-livo-oem-boot-probe.yaml'),
    Path('examples/gree-livo-gen3-refined-discovery.yaml'),
    Path('examples/gree-livo-gen3-full-power-discovery.yaml'),
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
PY

for example in \
  examples/gree-livo-gen3-receive-only.yaml \
  examples/gree-livo-gen3-poll-only.yaml \
  examples/gree-livo-gen3-control.yaml \
  examples/.validation-gree-livo-gen3-refined-discovery.yaml \
  examples/.validation-gree-livo-gen3-full-power-discovery.yaml \
  examples/.validation-gree-livo-oem-boot-probe.yaml; do
  esphome config "$example"
done
