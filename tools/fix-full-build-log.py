#!/usr/bin/env python3
"""XSSH nxdk port fix for the 25 SSIZE_T/io.h compile failures.

Usage: python3 fix-xssh-full-log-2026-10-28.py /path/to/xbox-nxdk-ssh
Patches Makefile + the libssh2 vendor patcher and applies that vendor patch.
Safe to run twice. Does not replace your xssh.cfg or other application files.
"""
from pathlib import Path
import subprocess
import sys

MSVC_TYPES_ORIGINAL = "#ifdef _MSC_VER\ntypedef unsigned char uint8_t;"
MSVC_TYPES_PATCH = "#if defined(_MSC_VER) && !defined(NXDK)\ntypedef unsigned char uint8_t;"
MSVC_FILES_ORIGINAL = "#if defined(_MSC_VER) && !defined(_WIN32_WCE)"
MSVC_FILES_PATCH = "#if defined(_MSC_VER) && !defined(_WIN32_WCE) && !defined(NXDK)"


def exactly_once(s, old, new, name):
    if new in s:
        return s
    found = s.count(old)
    if found != 1:
        raise RuntimeError(f"{name}: expected exactly one occurrence of {old!r} (found {found})")
    return s.replace(old, new, 1)


def fix(root):
    makefile = root / 'Makefile'
    script = root / 'tools' / 'patch-libssh2-nxdk.py'
    if not makefile.is_file() or not script.is_file():
        raise RuntimeError('Not an XSSH project directory (Makefile or libssh2 patcher missing).')

    # The xbox Clang target may define _MSC_VER. libssh2 must instead use
    # portable integer, ssize_t, and file-stat compatibility branches.
    original = makefile.read_text()
    if '-DNXDK=1' not in original:
        updated = exactly_once(original, 'NXDK_CFLAGS += ', 'NXDK_CFLAGS += -DNXDK=1 ', makefile)
        makefile.write_text(updated)
        print('Updated: Makefile (added -DNXDK=1)')
    else:
        print('Already fixed: Makefile')

    original = script.read_text()
    updated = original
    if 'MSVC_TYPES_PATCH =' not in updated:
        defs = (
            '\n# nxdk clang may define _MSC_VER without Microsoft runtime types.\n'
            f'MSVC_TYPES_ORIGINAL = {MSVC_TYPES_ORIGINAL!r}\n'
            f'MSVC_TYPES_PATCH = {MSVC_TYPES_PATCH!r}\n'
            f'MSVC_FILES_ORIGINAL = {MSVC_FILES_ORIGINAL!r}\n'
            f'MSVC_FILES_PATCH = {MSVC_FILES_PATCH!r}\n'
        )
        updated = exactly_once(updated, "SFTP_ORIGINAL = ", defs + "\nSFTP_ORIGINAL = ", script)
    if 'content = replace_one(content, MSVC_TYPES_ORIGINAL, MSVC_TYPES_PATCH, inc)' not in updated:
        old = '    content = replace_one(content, FILE_SIZE_ORIGINAL, FILE_SIZE_PATCH, inc)\n'
        new = (old + '    content = replace_one(content, MSVC_TYPES_ORIGINAL, MSVC_TYPES_PATCH, inc)\n'
                   '    content = replace_one(content, MSVC_FILES_ORIGINAL, MSVC_FILES_PATCH, inc)\n')
        updated = exactly_once(updated, old, new, script)
    if updated != original:
        script.write_text(updated)
        print('Updated: tools/patch-libssh2-nxdk.py')
    else:
        print('Already fixed: libssh2 dependency patcher')

    vendor_header = root / 'third_party' / 'libssh2' / 'include' / 'libssh2.h'
    if vendor_header.is_file():
        subprocess.run([sys.executable, str(script), str(root)], cwd=root, check=True)
    else:
        print('Vendor libssh2 not downloaded. Run ./tools/fetch-deps.sh before building.')

    print('Fix applied. Rebuild with: make clean && make NXDK_DIR="$HOME/nxdk" -j4')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('Usage: python3 fix-xssh-full-log-2026-10-28.py /path/to/xbox-nxdk-ssh')
    try:
        fix(Path(sys.argv[1]).expanduser().resolve())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        sys.exit(f'ERROR: {exc}')
