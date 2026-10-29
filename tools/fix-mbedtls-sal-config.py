#!/usr/bin/env python3
"""Work around missing Microsoft SAL annotations in nxdk's Mbed TLS 3.6.5 build.

Usage: python3 tools/fix-mbedtls-sal-config.py [project-directory]
Only edits include/mbedtls_config_xbox.h. Safe to rerun.
"""
from pathlib import Path
import re
import sys


def fix(root: Path) -> bool:
    path = root / 'include' / 'mbedtls_config_xbox.h'
    if not path.is_file():
        raise FileNotFoundError(path)
    text = path.read_text(encoding='utf-8')
    if re.search(r'(?m)^\s*#\s*define\s+MBEDTLS_CHECK_RETURN\b', text):
        return False
    marker = '#define MBEDTLS_CONFIG_VERSION 0x03000000\n'
    if text.count(marker) != 1:
        raise RuntimeError('Unsupported Mbed TLS config structure; edit by hand')
    addition = ("/* nxdk Clang/MSVC target has no Microsoft SDK <sal.h>. */\n"
                "#define MBEDTLS_CHECK_RETURN __attribute__((__warn_unused_result__))\n")
    path.write_text(text.replace(marker, marker + addition, 1), encoding='utf-8')
    return True


if __name__ == '__main__':
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
    try:
        changed = fix(root.resolve())
    except (OSError, RuntimeError) as exc:
        sys.exit(f'ERROR: {exc}')
    print('Mbed TLS SAL compatibility: ' + ('patched' if changed else 'already patched'))
