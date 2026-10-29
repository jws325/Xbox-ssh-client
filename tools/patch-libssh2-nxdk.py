#!/usr/bin/env python3
"""Apply the libssh2 1.11.1 SSH-terminal-only nxdk portability patch.

Run after tools/fetch-deps.sh, or in an existing checkout:
    python3 tools/patch-libssh2-nxdk.py

This does NOT implement POSIX stat/filesystem APIs. The XSSH build does not
compile libssh2 SCP/SFTP modules. All modifications are conditional on NXDK.
"""
from pathlib import Path
import sys

STAT_INCLUDE = '#include <sys/stat.h>'
STAT_PATCH_A = '''#if defined(NXDK)
/* SSH terminal only. SCP/SFTP aren't built: no POSIX stat implementation. */
struct stat;
#else
#include <sys/stat.h>
#endif'''
STAT_PATCH_B = '''#if defined(NXDK)
/* nxdk lacks sys/stat.h. The SSH terminal does not build libssh2 SCP or SFTP.
 * The public libssh2 API only needs this type as an opaque declaration.
 * Do not dereference this type or enable SCP without a real stat implementation.
 */
struct stat;
#else
#include <sys/stat.h>
#endif'''
TYPE_INCLUDE = '#include <sys/types.h>'
TYPE_PATCH = '''#if defined(NXDK)
/* nxdk's PDCLib is not a POSIX libc. On the 32-bit Xbox, a signed
 * pointer-sized type is compatible with libssh2's ssize_t callbacks. */
#include <stddef.h>
#include <time.h>
#if !defined(_SSIZE_T_DEFINED) && !defined(_SSIZE_T) && !defined(__ssize_t_defined)
typedef ptrdiff_t ssize_t;
#define _SSIZE_T_DEFINED 1
#endif
#else
#include <sys/types.h>
#endif'''
TYPE_PATCH_OLD = TYPE_PATCH.replace('#include <time.h>\n', '')
FILE_SIZE_ORIGINAL = 'typedef struct stat libssh2_struct_stat;\ntypedef off_t libssh2_struct_stat_size;'
FILE_SIZE_PATCH = '''typedef struct stat libssh2_struct_stat;
#if defined(NXDK)
/* libssh2 requires this typedef in its public API, but XSSH does not
 * use SCP or SFTP. Do not fabricate a system-wide off_t. */
typedef long long libssh2_struct_stat_size;
#else
typedef off_t libssh2_struct_stat_size;
#endif'''
# nxdk targets Windows-compatible Xbox ABI and can expose _MSC_VER even
# when compiling with clang. libssh2 must use its portable typedef/socket and
# file-size code paths, not Visual Studio's SSIZE_T / <io.h> branches.
MSVC_TYPES_ORIGINAL = '#ifdef _MSC_VER\ntypedef unsigned char uint8_t;'
MSVC_TYPES_PATCH = '#if defined(_MSC_VER) && !defined(NXDK)\ntypedef unsigned char uint8_t;'
MSVC_FILES_ORIGINAL = '#if defined(_MSC_VER) && !defined(_WIN32_WCE)'
MSVC_FILES_PATCH = '#if defined(_MSC_VER) && !defined(_WIN32_WCE) && !defined(NXDK)'

TIME_INCLUDE_OLD = "#include <stddef.h>\n#if !defined(_SSIZE_T_DEFINED)"
TIME_INCLUDE_NEW = ("#include <stddef.h>\n"
                    "#include <time.h>\n"
                    "#if !defined(_SSIZE_T_DEFINED)")

SFTP_ORIGINAL = '#ifndef _WIN32\n#include <unistd.h>\n#endif'
SFTP_PATCH = '#if !defined(_WIN32) && !defined(NXDK)\n#include <unistd.h>\n#endif'


def replace_one(text, before, after, path):
    if after in text:
        return text
    if text.count(before) != 1:
        raise RuntimeError(f'{path}: expected exactly one matching source block, found {text.count(before)}; libssh2 version may differ')
    return text.replace(before, after, 1)


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



def patch_session_socket_mode(root: Path):
    """Use nxdk's lwIP ioctlsocket(FIONBIO), not POSIX fcntl.

    XSSH hands libssh2 a fresh blocking socket, so we can reliably switch
    it to nonblocking for the handshake and restore it on session teardown.
    """
    session = root / "src" / "session.c"
    if not session.is_file():
        raise RuntimeError(f"Missing {session}; run tools/fetch-deps.sh first")
    original = session.read_text()
    content = original
    content = replace_one(content, '#include <fcntl.h>',
                          '#if !defined(NXDK)\n#include <fcntl.h>\n#endif', session)

    before = """session_nonblock(libssh2_socket_t sockfd,   /* operate on this */
                 int nonblock /* TRUE or FALSE */ )
{
#ifdef HAVE_O_NONBLOCK"""
    after = """session_nonblock(libssh2_socket_t sockfd,   /* operate on this */
                 int nonblock /* TRUE or FALSE */ )
{
#if defined(NXDK)
    /* nxdk's lwIP provides the Winsock-compatible FIONBIO ioctl. */
    unsigned long mode = nonblock ? 1UL : 0UL;
    return ioctlsocket(sockfd, FIONBIO, &mode);
#elif defined(HAVE_O_NONBLOCK)"""
    content = replace_one(content, before, after, session)

    before = """get_socket_nonblocking(libssh2_socket_t sockfd)
{                               /* operate on this */
#ifdef HAVE_O_NONBLOCK"""
    after = """get_socket_nonblocking(libssh2_socket_t sockfd)
{                               /* operate on this */
#if defined(NXDK)
    /* XSSH passes a fresh, initially blocking socket to libssh2.
     * Always enable FIONBIO for SSH handshake. This port expects the
     * same socket initialization behavior if reused elsewhere. */
    (void)sockfd;
    return 0;
#elif defined(HAVE_O_NONBLOCK)"""
    content = replace_one(content, before, after, session)
    if content != original:
        session.write_text(content)
        print('Patched:', session)
    else:
        print('Already patched:', session)


def patch_mbedtls_file_io(project: Path):
    """Enable Mbed TLS file key parser referenced from libssh2's backend."""
    config = project / 'include' / 'mbedtls_config_xbox.h'
    if not config.is_file():
        raise RuntimeError(f'Missing {config}')
    content = config.read_text()
    if '#define MBEDTLS_FS_IO' not in content:
        if '#define MBEDTLS_PLATFORM_C\n' not in content:
            raise RuntimeError(f'{config}: missing MBEDTLS_PLATFORM_C')
        content = content.replace('#define MBEDTLS_PLATFORM_C\n',
            '/* libssh2 references the mbedtls_pk_parse_keyfile() API. */\n'
            '#define MBEDTLS_FS_IO\n#define MBEDTLS_PLATFORM_C\n', 1)
        config.write_text(content)
        print('Patched:', config)
    else:
        print('Already patched:', config)


def patch(root: Path):
    patch_chacha_bsd_types(root)
    patch_session_socket_mode(root)
    patch_mbedtls_file_io(root.parent.parent)
    inc = root / 'include' / 'libssh2.h'
    priv = root / 'src' / 'libssh2_priv.h'
    sftp = root / 'include' / 'libssh2_sftp.h'
    if not inc.is_file():
        raise RuntimeError(f'Missing {inc}; run tools/fetch-deps.sh first')
    content = inc.read_text()
    if STAT_PATCH_A not in content and STAT_PATCH_B not in content:
        content = replace_one(content, STAT_INCLUDE, STAT_PATCH_B, inc)
    # Upgrade an existing nxdk patch in place before touching the original
    # POSIX include: both the old and new blocks contain sys/types.h in their
    # non-nxdk branches, so naive replacement would nest another patch block.
    if TYPE_PATCH not in content:
        if TYPE_PATCH_OLD in content:
            content = content.replace(TYPE_PATCH_OLD, TYPE_PATCH, 1)
        else:
            content = replace_one(content, TYPE_INCLUDE, TYPE_PATCH, inc)
    content = replace_one(content, FILE_SIZE_ORIGINAL, FILE_SIZE_PATCH, inc)
    content = replace_one(content, MSVC_TYPES_ORIGINAL, MSVC_TYPES_PATCH, inc)
    content = replace_one(content, MSVC_FILES_ORIGINAL, MSVC_FILES_PATCH, inc)
    if content != inc.read_text():
        inc.write_text(content)
        print('Patched:', inc)
    else:
        print('Already patched:', inc)

    # The previously generated libssh2_config.h asserts POSIX socket headers
    # exist on nxdk. libssh2 uses lwIP sockets here, not sys/socket.h.
    config = root.parent.parent / 'include' / 'libssh2_config.h'
    if config.is_file():
        cfg = config.read_text()
        cfg2 = cfg
        for macro in ('HAVE_SYS_SOCKET_H', 'HAVE_SYS_SELECT_H'):
            original = f'#define {macro} 1'
            guarded = f'#ifndef NXDK\n{original}\n#endif'
            if guarded not in cfg2:
                cfg2 = cfg2.replace(original, guarded)
        if cfg2 != cfg:
            config.write_text(cfg2)
            print('Patched:', config)

    # Match the lwIP socket header used by XSSH itself.
    if priv.is_file():
        txt = priv.read_text()
        marker = '#include "libssh2_setup.h"'
        block = '''#include "libssh2_setup.h"
#ifdef NXDK
#include <lwip/sockets.h>
#endif'''
        txt2 = replace_one(txt, marker, block, priv)
        if txt2 != txt:
            priv.write_text(txt2)
            print('Patched:', priv)

    # libssh2_priv.h includes libssh2_sftp.h even though XSSH doesn't
    # build SFTP. Avoid the POSIX-only unistd.h inclusion on nxdk.
    if sftp.is_file():
        txt = sftp.read_text()
        txt2 = replace_one(txt, SFTP_ORIGINAL, SFTP_PATCH, sftp)
        if txt2 != txt:
            sftp.write_text(txt2)
            print('Patched:', sftp)


if __name__ == '__main__':
    project = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
    libssh2 = project / 'third_party' / 'libssh2'
    try:
        patch(libssh2)
    except (OSError, RuntimeError) as error:
        print('ERROR:', error, file=sys.stderr)
        sys.exit(1)
