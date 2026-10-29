#!/usr/bin/env python3
"""Fix libssh2 MSVC-only SSIZE_T and io.h branches when compiling on nxdk.

Usage:
    python3 fix-xssh-libssh2-msvc-path.py /path/to/xbox-nxdk-ssh

Patches the already-downloaded third_party/libssh2/include/libssh2.h and
updates tools/patch-libssh2-nxdk.py so repeated dependency patching is safe.
The patch is idempotent and deliberately fails if the expected libssh2 1.11.1
source structure is absent.
"""
import sys
from pathlib import Path

TYPES = ('#ifdef _MSC_VER\ntypedef unsigned char uint8_t;',
         '#if defined(_MSC_VER) && !defined(NXDK)\ntypedef unsigned char uint8_t;')
FILES = ('#if defined(_MSC_VER) && !defined(_WIN32_WCE)',
         '#if defined(_MSC_VER) && !defined(_WIN32_WCE) && !defined(NXDK)')
CONSTANTS = '''# nxdk clang may advertise _MSC_VER, but its libc does not provide
# Visual Studio's SSIZE_T and <io.h>. Use portable libssh2 definitions.
MSVC_TYPES_ORIGINAL = '#ifdef _MSC_VER\\ntypedef unsigned char uint8_t;'
MSVC_TYPES_PATCH = '#if defined(_MSC_VER) && !defined(NXDK)\\ntypedef unsigned char uint8_t;'
MSVC_FILES_ORIGINAL = '#if defined(_MSC_VER) && !defined(_WIN32_WCE)'
MSVC_FILES_PATCH = '#if defined(_MSC_VER) && !defined(_WIN32_WCE) && !defined(NXDK)'

'''


def exact_once(text, before, after, path):
    if after in text:
        return text
    n = text.count(before)
    if n != 1:
        raise RuntimeError(f'{path}: expected one match for {before!r}, found {n}')
    return text.replace(before, after, 1)


def main(root):
    hdr = root / 'third_party' / 'libssh2' / 'include' / 'libssh2.h'
    patcher = root / 'tools' / 'patch-libssh2-nxdk.py'
    if not patcher.exists():
        raise RuntimeError(f'Missing {patcher}')
    p = patcher.read_text()
    if 'MSVC_TYPES_ORIGINAL =' not in p:
        marker = 'SFTP_ORIGINAL = '
        if p.count(marker) != 1:
            raise RuntimeError('Unexpected original libssh2 patch script: cannot find SFTP_ORIGINAL')
        p = p.replace(marker, CONSTANTS + marker, 1)
        needle = '    content = replace_one(content, FILE_SIZE_ORIGINAL, FILE_SIZE_PATCH, inc)\n'
        replacement = needle + (
            '    content = replace_one(content, MSVC_TYPES_ORIGINAL, MSVC_TYPES_PATCH, inc)\n'
            '    content = replace_one(content, MSVC_FILES_ORIGINAL, MSVC_FILES_PATCH, inc)\n'
        )
        p = exact_once(p, needle, replacement, patcher)
        patcher.write_text(p)
        print('Updated automatic dependency patcher:', patcher)
    else:
        print('Already updated:', patcher)

    if hdr.exists():
        content = hdr.read_text()
        content = exact_once(content, *TYPES, hdr)
        content = exact_once(content, *FILES, hdr)
        hdr.write_text(content)
        print('Patched libssh2 header:', hdr)
    else:
        print('libssh2 has not been downloaded; run ./tools/fetch-deps.sh later.')


if __name__ == '__main__':
    folder = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
    if len(sys.argv) > 2:
        sys.exit('Usage: fix-xssh-libssh2-msvc-path.py [xssh-project-directory]')
    try:
        main(folder)
    except (OSError, RuntimeError) as exc:
        sys.exit(f'ERROR: {exc}')
