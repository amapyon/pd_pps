#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13a4_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic'
cc $CFLAGS -Isrc -o "$out" tests/test_m13a4_hr_vbus.c src/pd_hr_vbus.c
"$out"
cc $CFLAGS -Isrc -o "$out" tests/test_m13a4_monitor_integration.c \
  src/pd_vbus_monitor.c src/pd_vbus_history.c src/pd_hr_vbus.c
"$out"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only \
  -Itests/mocks_main -Isrc src/main.c
printf '%s\n' 'M13a-4 main.c host syntax: PASS'
