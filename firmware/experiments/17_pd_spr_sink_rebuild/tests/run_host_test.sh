#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc src/pd_policy.c tests/test_policy.c -o tests/test_policy
./tests/test_policy
rm -f tests/test_policy
