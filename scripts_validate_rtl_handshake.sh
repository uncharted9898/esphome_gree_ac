#!/usr/bin/env bash
set -euo pipefail

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/test_rtl_handshake.cpp \
  -o /tmp/test_rtl_handshake
/tmp/test_rtl_handshake

python3 -m py_compile tools/firmware_research/audit_rtl8720cf_handshake.py

git diff --check
