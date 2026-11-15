#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT
SRC=(src/m5_pd_snapshot.c src/m5_readonly.c src/m5_session.c src/m5_bridge_runtime.c src/m5_usb_io.c src/m5_recovery_nb.c)
FLAGS=(-std=c11 -Wall -Wextra -Werror -O1 -g -Isrc)
"$CC" "${FLAGS[@]}" "${SRC[@]}" tests/test_m5_integration.c -o "$TEMP_DIR/test27"
"$TEMP_DIR/test27"
"$CC" "${FLAGS[@]}" src/m5_pd_snapshot.c tests/test_existing_snapshot.c -o "$TEMP_DIR/test26"
"$TEMP_DIR/test26"
python3 -m unittest discover -s tests -p test_wire_reference.py -v
