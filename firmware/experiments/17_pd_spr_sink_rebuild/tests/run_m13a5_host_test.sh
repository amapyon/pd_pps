#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13a5_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
cc $flags -Isrc -o "$out" tests/test_m13a5_hr_bench.c src/pd_hr_bench.c
"$out"
cc $flags -Itests/mocks -Isrc -o "$out" tests/test_m13a5_hard_reset_tx.c src/pd_policy.c
"$out"
cc $flags -fsyntax-only -Itests/mocks_main -Isrc src/main.c
printf '%s\n' 'M13a-5 main.c host syntax: PASS'
