#!/bin/sh
# M13a: production code with mocked registers. No MCU flashing involved.
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13a_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -Itests/mocks -Isrc \
   -o "$out" tests/test_m13a_hardreset.c src/pd_policy.c
"$out"
