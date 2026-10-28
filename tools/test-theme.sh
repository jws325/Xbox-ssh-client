#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Iinclude src/theme.c tools/test-theme.c -o "$TMP/test-theme"
"$TMP/test-theme"
