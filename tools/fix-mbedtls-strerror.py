#!/usr/bin/env python3
"""Fix XSSH/nxdk missing mbedtls_strerror at link time.

Usage:
    python3 fix-xssh-mbedtls-strerror.py /path/to/xbox-nxdk-ssh

Safe to run more than once. Modifies only the local XSSH Makefile and
include/mbedtls_config_xbox.h; does not touch upstream Mbed TLS sources.
"""
import argparse
from pathlib import Path
import re
import sys


def fix(project: Path) -> bool:
    project = Path(project)
    makefile = project / 'Makefile'
    config = project / 'include' / 'mbedtls_config_xbox.h'
    if not makefile.is_file() or not config.is_file():
        raise ValueError(f'{project} is not an XSSH source directory (Makefile or config missing)')

    mk = makefile.read_text(encoding='utf-8')
    cfg = config.read_text(encoding='utf-8')

    # Only patch the Mbed TLS source list, not the libssh2 or app source list.
    match = re.search(r'(?ms)^TLS_C\s*=\s*(.*?)(?=^\S[^\n]*\s*=|^#|^include\s|\Z)', mk)
    if not match:
        raise ValueError('Makefile does not contain a recognizable TLS_C source list')
    block = match.group(1)
    sources = re.findall(r'\b[a-z][a-z0-9_]*\b', block.replace('\\\n', ' '))
    if 'error' not in sources:
        if 'entropy' not in sources:
            raise ValueError('TLS_C does not contain expected entropy source; refusing to guess where to add error.c')
        # Keep alphabetical order near entropy (error.c follows entropy_poll.c).
        revised = re.sub(r'\bentropy_poll\b', 'entropy_poll error', block, count=1)
        if revised == block:
            raise ValueError('TLS_C does not contain entropy_poll source')
        mk = mk[:match.start(1)] + revised + mk[match.end(1):]

    if not re.search(r'(?m)^\s*#\s*define\s+MBEDTLS_ERROR_C\b', cfg):
        marker = '#define MBEDTLS_ENTROPY_C\n'
        if cfg.count(marker) != 1:
            raise ValueError('Missing unique MBEDTLS_ENTROPY_C marker in Mbed TLS config')
        cfg = cfg.replace(marker,
                          '/* Provide mbedtls_strerror() used by the libssh2 Mbed TLS backend. */\n'
                          '#define MBEDTLS_ERROR_C\n' + marker, 1)

    changed = False
    if makefile.read_text(encoding='utf-8') != mk:
        makefile.write_text(mk, encoding='utf-8')
        changed = True
        print('Updated Makefile: compiling third_party/mbedtls/library/error.c')
    if config.read_text(encoding='utf-8') != cfg:
        config.write_text(cfg, encoding='utf-8')
        changed = True
        print('Updated Mbed TLS configuration: MBEDTLS_ERROR_C enabled')
    if not changed:
        print('Already patched: no changes needed')
    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', nargs='?', default='.', type=Path,
                        help='Path to XSSH source folder (defaults to current directory)')
    args = parser.parse_args()
    try:
        fix(args.project.resolve())
    except (OSError, ValueError) as exc:
        print('ERROR:', exc, file=sys.stderr)
        sys.exit(1)
