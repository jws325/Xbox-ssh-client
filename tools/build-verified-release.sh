#!/usr/bin/env bash
# Build everything from THIS source tree, refusing to distribute a stale XBE.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
NXDK_DIR="${NXDK_DIR:-$HOME/nxdk}"
if [[ ! -f "$NXDK_DIR/Makefile" ]]; then
  echo "ERROR: nxdk not found at $NXDK_DIR; set NXDK_DIR=/path/to/nxdk" >&2
  exit 1
fi
if [[ -f "$NXDK_DIR/bin/activate" ]]; then
  eval "$("$NXDK_DIR/bin/activate" -s)"
fi
if ! command -v nxdk-cc >/dev/null 2>&1; then
  echo 'ERROR: nxdk-cc not found. Activate nxdk in this shell.' >&2
  exit 1
fi
if [[ ! -d third_party/libssh2/src || ! -d third_party/mbedtls/library ]]; then
  echo 'ERROR: dependencies missing: run ./tools/fetch-deps.sh first' >&2
  exit 1
fi
# Reject blank or corrupted bitmap fonts before cross-compiling.
python3 tools/check-font-atlas.py
python3 tools/test-antialiasing.py
# Leave the existing seed alone, but never leave an old XBE in release/
# if the new compilation or verification fails.
rm -f release/default.xbe
jobs="${JOBS:-4}"
make -B -j"$jobs" NXDK_DIR="$NXDK_DIR" release
python3 tools/verify-built-xbe.py release/default.xbe
if ! cmp -s bin/default.xbe release/default.xbe; then
  echo 'ERROR: packaged XBE differs from compiled XBE' >&2
  exit 1
fi
echo 'PASS: release/default.xbe is verified and byte-identical to bin/default.xbe'
sha256sum bin/default.xbe release/default.xbe
printf '\nCopy release/default.xbe and release/xssh.cfg to the Xbox.\n'
printf 'For a NEW install copy release/xssh-seed.bin too; preserve the existing Xbox seed on upgrades.\n'
