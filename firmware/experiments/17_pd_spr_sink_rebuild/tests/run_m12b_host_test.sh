#!/bin/sh
# Build & execute the real PHY driver with fake CH32 hardware on a host GCC.
# Does not use WCH-LinkE, minichlink, ch32fun, USB-C, or flash the MCU.
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m12b_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -Itests/mocks -Isrc \
    -o "$out" tests/test_phy_crc_retry.c src/pd_policy.c
"$out"
