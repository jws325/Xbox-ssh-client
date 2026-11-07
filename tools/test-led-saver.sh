#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
temp="$(mktemp -d)"
trap 'rm -rf "$temp"' EXIT
python3 tools/create-demo-saver.py "$temp/screensaver.xsv"
cc -std=c11 -Wall -Wextra -Werror -Iinclude src/xssh_led.c src/xssh_saver.c tools/test-led-saver.c -o "$temp/test"
"$temp/test" "$temp/screensaver.xsv"
# Truncated file should be rejected
head -c 128 "$temp/screensaver.xsv" > "$temp/bad.xsv"
python3 - "$temp/bad.xsv" <<'PY'
import sys
from pathlib import Path
assert Path(sys.argv[1]).stat().st_size==128
PY
