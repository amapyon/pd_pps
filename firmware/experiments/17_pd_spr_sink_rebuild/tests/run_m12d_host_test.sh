#!/bin/sh
# M12d uses the actual production PHY and Policy Engine with register mocks.
# This is a HOST-ONLY test; does not build or flash the MCU.
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m12d_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -Itests/mocks -Isrc \
   -o "$out" tests/test_ams_arbitration.c src/pd_policy.c
"$out"
