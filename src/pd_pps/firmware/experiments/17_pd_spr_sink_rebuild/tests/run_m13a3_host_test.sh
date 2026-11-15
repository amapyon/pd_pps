#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_m13a3_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -Isrc \
    -o "$out" tests/test_m13a3_vbus_loss_detector.c src/pd_vbus_monitor.c src/pd_vbus_history.c src/pd_hr_vbus.c
"$out"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -Itests/mocks -Isrc \
    -o "$out" tests/test_m13a3_vbus_contract_abort.c src/pd_policy.c
"$out"
cc -std=c11 -O1 -Wall -Wextra -Werror -pedantic -fsyntax-only \
    -Itests/mocks_main -Isrc src/main.c
printf '%s\n' 'M13a-3 main.c host syntax: PASS'
