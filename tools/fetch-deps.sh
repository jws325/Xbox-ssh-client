#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p third_party
if [ ! -d third_party/libssh2/.git ]; then
  git clone --depth 1 --branch libssh2-1.11.1 https://github.com/libssh2/libssh2.git third_party/libssh2
fi
if [ ! -d third_party/mbedtls/.git ]; then
  git clone --depth 1 --branch v3.6.5 https://github.com/Mbed-TLS/mbedtls.git third_party/mbedtls
fi
python3 tools/patch-libssh2-nxdk.py
printf 'Dependencies fetched and nxdk libssh2 header patched. Build with: make NXDK_DIR=/path/to/nxdk\n'
