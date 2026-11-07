#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
out="$(mktemp /tmp/xssh-test-connect-XXXXXX)"
trap 'rm -f "$out"' EXIT
"$CC" -std=c11 -Wall -Wextra -Werror -Iinclude tools/test-connect-form.c src/connect_form.c src/known_hosts.c -o "$out"
"$out"
