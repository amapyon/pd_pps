#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13b_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
# Negative build guard: normal main must not invoke bench Hard Reset or
# synthetic Get_Source_Cap. This is deliberate, not a runtime feature switch.
if grep -Eq 'phy_send_hard_reset_once[[:space:]]*\(|pe_request_source_caps[[:space:]]*\(' src/main.c; then
    echo 'FAIL: bench-only PD stimulus in normal main.c' >&2
    exit 1
fi
if grep -Eq 'src/pd_hr_bench\.c|src/pd_hr_pps_probe\.c' Makefile; then
    echo 'FAIL: bench-only code is still in normal Makefile' >&2
    exit 1
fi
cc $flags -Itests/mocks -Isrc -o "$out" \
  tests/test_m13b_source_hard_reset.c \
  src/pd_policy.c src/pd_hr_recovery.c src/pd_hr_vbus.c
"$out"
cc $flags -Isrc -o "$out" tests/test_m13b_hr_result_flags.c \
  src/pd_hr_vbus.c src/pd_vbus_history.c
"$out"
cc $flags -fsyntax-only -Itests/mocks_main -Isrc src/main.c
printf '%s\n' 'M13b normal main syntax + no bench stimulus: PASS'
