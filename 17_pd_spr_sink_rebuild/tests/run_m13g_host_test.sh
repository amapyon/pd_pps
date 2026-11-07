#!/bin/sh
# M13g: state-transition bounds and real PE/PHY stop tests (host mocks only).
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13g_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
cc $flags -Isrc -o "$out" tests/test_m13g_state_transitions.c src/pd_policy.c
"$out"
cc $flags -Itests/mocks -Isrc -o "$out" \
    tests/test_m13g_phy_fail_closed.c src/pd_policy.c
"$out"
# No fault injector or Sink-initiated Hard Reset included in production main.
! grep -Eq 'phy_send_hard_reset_once[[:space:]]*\(|pe_request_source_caps[[:space:]]*\(' src/main.c
! grep -Eq 'test_m13g|src/pd_hr_bench\.c|src/pd_hr_pps_probe\.c' Makefile
printf '%s\n' 'M13g host-only test isolation: PASS'
