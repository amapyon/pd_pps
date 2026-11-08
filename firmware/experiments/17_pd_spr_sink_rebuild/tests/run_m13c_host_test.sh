#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13c_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
cc $flags -Isrc -o "$out" tests/test_m13c_stability.c src/pd_stability.c
"$out"
cc $flags -Itests/mocks -Isrc -o "$out" \
    tests/test_m13c_fail_closed.c src/pd_policy.c
"$out"
cc $flags -fsyntax-only -Itests/mocks_main -Isrc src/main.c
# Normal-mode regression: no bench stimulus introduced by M13c.
! grep -Eq 'phy_send_hard_reset_once[[:space:]]*\(|pe_request_source_caps[[:space:]]*\(' src/main.c
! grep -Eq 'src/pd_hr_bench\.c|src/pd_hr_pps_probe\.c' Makefile
printf '%s\n' 'M13c syntax and normal-mode no-stimulus guard: PASS'
