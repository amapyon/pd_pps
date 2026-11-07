#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_vbus_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -Isrc \
    -o "$out" tests/test_vbus_history.c src/pd_vbus_history.c
"$out"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -Isrc \
    -o "$out" tests/test_vbus_monitor.c src/pd_vbus_monitor.c src/pd_vbus_history.c src/pd_hr_vbus.c
"$out"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -Itests/mocks_adc -Isrc \
    -o "$out" tests/test_adc_async_driver.c src/adc_voltage.c
"$out"
cc -std=c11 -O1 -Wall -Wextra -Werror -pedantic -fsyntax-only -Itests/mocks_main -Isrc src/main.c
printf '%s\n' 'M13a-2 main.c compile check (host mock): PASS'
