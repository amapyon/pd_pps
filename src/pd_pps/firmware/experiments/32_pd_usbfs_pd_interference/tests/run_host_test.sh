#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
cc="${CC:-gcc}"
obj="$(mktemp -d)"
trap 'rm -rf "$obj"' EXIT HUP INT TERM
"$cc" -std=c11 -Wall -Wextra -Werror -pedantic -g -Isrc \
  src/m5_bridge_runtime.c src/m5_pd_snapshot.c src/m5_readonly.c \
  src/m5_session.c src/m5_recovery_nb.c src/m5_usb_io.c \
  src/m28_transport.c src/m28_runtime.c \
  tests/test_m28_runtime.c -o "$obj/test_m28"
"$obj/test_m28"
