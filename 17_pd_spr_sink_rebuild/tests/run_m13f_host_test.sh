#!/bin/sh
# M13f: production Policy Engine/PHY only, deterministic host clock and register mocks.
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13f_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
cc $flags -Isrc -o "$out" tests/test_m13f_timing.c src/pd_policy.c
"$out"
cc $flags -Itests/mocks -Isrc -o "$out" \
    tests/test_m13f_message_id.c src/pd_policy.c
"$out"
# M13f must not add testing hooks to firmware or change the default 7V/1A request.
! grep -Eq 'phy_send_hard_reset_once[[:space:]]*\(|pe_request_source_caps[[:space:]]*\(' src/main.c
! grep -Eq 'test_m13f|src/pd_hr_bench\.c|src/pd_hr_pps_probe\.c' Makefile
printf '%s\n' 'M13f host-only test isolation: PASS'
