#!/bin/sh
# M13e: C compiler + mock USBPD/ADC. No physical charger, no firmware flags.
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13e_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
cc $flags -Itests/mocks -Isrc -o "$out" \
    tests/test_m13e_fault_injection.c \
    src/pd_policy.c src/pd_vbus_monitor.c src/pd_vbus_history.c \
    src/pd_hr_vbus.c src/pd_stop_diag.c src/pd_stability.c
"$out"
# Guard that no bench-only injection hooks or reset stimulus are put in main.
! grep -Eq 'phy_send_hard_reset_once[[:space:]]*\(|pe_request_source_caps[[:space:]]*\(' src/main.c
! grep -Eq 'src/pd_hr_bench\.c|src/pd_hr_pps_probe\.c|test_m13e_fault_injection' Makefile
printf '%s\n' 'M13e production build excludes fault injector: PASS'
