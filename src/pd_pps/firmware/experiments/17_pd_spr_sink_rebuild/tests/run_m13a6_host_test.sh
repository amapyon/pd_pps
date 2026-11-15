#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13a6_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
cc $flags -Isrc -o "$out" tests/test_m13a6_hr_recovery_state.c \
  src/pd_hr_recovery.c src/pd_hr_vbus.c src/pd_policy.c
"$out"
cc $flags -Itests/mocks -Isrc -o "$out" tests/test_m13a6_phy_early_caps.c src/pd_policy.c
"$out"
cc $flags -fsyntax-only -Itests/mocks_main -Isrc src/main.c
printf '%s\n' 'M13a-6 main.c host syntax: PASS'
