#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp "${TMPDIR:-/tmp}/pd17_sensors_XXXXXX")"
trap 'rm -f "$out"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -Itests/mocks -Isrc \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -o "$out" tests/test_pd_sensors_logic.c src/pd_sensors.c src/adc_calibration.c
"$out"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -Isrc -o "$out" \
    tests/test_adc_calibration.c src/adc_calibration.c
"$out"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -Isrc -o "$out" \
    tests/test_ina226_units.c
"$out"
