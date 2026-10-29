#!/usr/bin/env python3
"""Fix libssh2 1.11.1 u_int/u_char errors in an existing XSSH nxdk checkout.

Usage: python3 fix-xssh-chacha-bsd-types.py [path/to/xbox-nxdk-ssh]
Requires that ./third_party/libssh2 has already been downloaded.
Repeat-safe; does not modify nxdk or remove ChaCha20-Poly1305 support.
"""
from pathlib import Path
import sys



def patch_chacha_bsd_types(root: Path):
    """Teach libssh2 1.11.1's OpenBSD ChaCha/Poly1305 headers nxdk types.

    These types are BSD aliases not supplied by nxdk's PDCLib.  Keep the
    definitions out of the Microsoft and POSIX builds, and install them before
    the first use in either header (including the standalone poly1305 case).
    """
    project = root.parent.parent
    (project / 'include').mkdir(parents=True, exist_ok=True)
    compat = project / 'include' / 'xssh_nxdk_bsd_types.h'
    compat_contents = '''/* Compatibility types for libssh2 1.11.1 OpenBSD ChaCha20/Poly1305. */
#ifndef XSSH_NXDK_BSD_TYPES_H
#define XSSH_NXDK_BSD_TYPES_H
#if defined(NXDK)
#include <stddef.h>
#include <stdint.h>
/* Original Xbox is a 32-bit target. These are the BSD aliases used
 * in the vendored algorithm signatures and state structures. */
typedef uint8_t u_char;
typedef uint32_t u_int;
#endif
#endif
'''
    if not compat.exists() or compat.read_text() != compat_contents:
        compat.write_text(compat_contents)
        print('Created:', compat)

    # Inject after existing vendor include guard, keeping upstream licenses
    # and avoiding global changes to standard library headers or other code.
    for name, guard in [('chacha.h', 'CHACHA_H'),
                        ('poly1305.h', 'POLY1305_H')]:
        path = root / 'src' / name
        if not path.is_file():
            raise RuntimeError(f'Missing {path}; run tools/fetch-deps.sh first')
        before = path.read_text()
        insertion = '#if defined(NXDK)\n#include "xssh_nxdk_bsd_types.h"\n#endif\n'
        if insertion not in before:
            line = f'#define {guard}\n'
            if before.count(line) != 1:
                raise RuntimeError(f'{path}: upstream header guard changed; refusing blind patch')
            after = before.replace(line, line + insertion, 1)
            path.write_text(after)
            print('Patched:', path)


def main():
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
    if not (root / 'Makefile').is_file() or not (root / 'tools' / 'patch-libssh2-nxdk.py').is_file():
        raise SystemExit(f'Not an XSSH project folder: {root}')
    patch_chacha_bsd_types(root / 'third_party' / 'libssh2')
    print('Done. Rebuild with: make NXDK_DIR="$HOME/nxdk" -j4')

if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError) as err:
        raise SystemExit(f'ERROR: {err}')
