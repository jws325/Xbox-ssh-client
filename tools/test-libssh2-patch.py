#!/usr/bin/env python3
"""Native smoke tests for libssh2 nxdk missing-POSIX-header port patch."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile

script = Path(__file__).parent / 'patch-libssh2-nxdk.py'
spec = importlib.util.spec_from_file_location('patch_libssh2_nxdk', script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as td:
    project = Path(td)
    root = project / 'third_party' / 'libssh2'
    (root / 'include').mkdir(parents=True)
    (root / 'src').mkdir(parents=True)
    (project / 'include').mkdir()
    hdr = root / 'include' / 'libssh2.h'
    hdr.write_text('''#ifndef LIBSSH2_H
#define LIBSSH2_H 1
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _MSC_VER
typedef unsigned char uint8_t;
typedef SSIZE_T ssize_t;
#else
#include <stdint.h>
#endif
#if defined(_MSC_VER) && !defined(_WIN32_WCE)
#define LIBSSH2_USE_WIN32_LARGE_FILES
#endif
#ifdef LIBSSH2_USE_WIN32_LARGE_FILES
#include <io.h>
typedef struct _stati64 libssh2_struct_stat;
typedef __int64 libssh2_struct_stat_size;
#endif
#ifndef LIBSSH2_STRUCT_STAT_SIZE_FORMAT
#define LIBSSH2_STRUCT_STAT_SIZE_FORMAT "%zd"
typedef struct stat libssh2_struct_stat;
typedef off_t libssh2_struct_stat_size;
#endif
extern ssize_t libssh2_read(char *buf, size_t len);
/* SCP isn't built but its prototypes require time_t in the public header. */
extern int libssh2_scp_send64(time_t mtime, time_t atime);
#endif
''')
    # The nxdk patcher also handles BSD type aliases used by the bundled
    # ChaCha20 and Poly1305 implementation.
    (root / 'src' / 'chacha.h').write_text('''#ifndef CHACHA_H
#define CHACHA_H
struct chacha_ctx { u_int input[16]; };
void chacha_keysetup(struct chacha_ctx *, const u_char *, u_int);
#endif
''')
    (root / 'src' / 'poly1305.h').write_text('''#ifndef POLY1305_H
#define POLY1305_H
void poly1305_auth(u_char out[16], const u_char *m, size_t len,
                   const u_char key[32]);
#endif
''')
    (root / 'src' / 'libssh2_priv.h').write_text('#include "libssh2_setup.h"\n#include <stdio.h>\n')
    (root / 'include' / 'libssh2_sftp.h').write_text('#ifndef _WIN32\n#include <unistd.h>\n#endif\n')
    (project / 'include' / 'libssh2_config.h').write_text('''#define HAVE_SELECT 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_SELECT_H 1
''')
    # Additional vendor session fixture for the nxdk nonblocking-socket patch.
    (root / 'src' / 'session.c').write_text('''#include <fcntl.h>
static int
session_nonblock(libssh2_socket_t sockfd,   /* operate on this */
                 int nonblock /* TRUE or FALSE */ )
{
#ifdef HAVE_O_NONBLOCK
    return fcntl(sockfd, F_SETFL, 0);
#else
    return 0;
#endif
}
static int
get_socket_nonblocking(libssh2_socket_t sockfd)
{                               /* operate on this */
#ifdef HAVE_O_NONBLOCK
    return fcntl(sockfd, F_GETFL, 0);
#else
    return 1;
#endif
}
''')
    (project / 'include' / 'mbedtls_config_xbox.h').write_text(
        '#define MBEDTLS_PLATFORM_C\n')
    module.patch(root)
    first = {p: p.read_bytes() for p in [hdr, root / 'src' / 'libssh2_priv.h', root / 'include' / 'libssh2_sftp.h', project / 'include' / 'libssh2_config.h']}
    module.patch(root)
    assert all(p.read_bytes() == b for p, b in first.items()), 'Patch must be idempotent'
    # Simulate a previously patched vendor header (without time.h), then
    # verify running the upgraded patcher repairs it in place.
    hdr.write_text(hdr.read_text().replace(
        '#include <stddef.h>\n#include <time.h>\n#if !defined(_SSIZE_T_DEFINED)',
        '#include <stddef.h>\n#if !defined(_SSIZE_T_DEFINED)', 1))
    assert '#include <time.h>' not in hdr.read_text()
    module.patch(root)
    assert '#include <time.h>' in hdr.read_text()
    assert 'typedef long long libssh2_struct_stat_size;' in hdr.read_text()
    assert '#if defined(_MSC_VER) && !defined(NXDK)' in hdr.read_text()
    assert '#if defined(_MSC_VER) && !defined(_WIN32_WCE) && !defined(NXDK)' in hdr.read_text()
    assert '#include <time.h>' in hdr.read_text()
    assert '#include "xssh_nxdk_bsd_types.h"' in (root / 'src' / 'chacha.h').read_text()
    assert '#include "xssh_nxdk_bsd_types.h"' in (root / 'src' / 'poly1305.h').read_text()
    assert 'typedef uint32_t u_int;' in (project / 'include' / 'xssh_nxdk_bsd_types.h').read_text()
    assert '#include <lwip/sockets.h>' in (root / 'src' / 'libssh2_priv.h').read_text()
    assert '#if !defined(_WIN32) && !defined(NXDK)' in (root / 'include' / 'libssh2_sftp.h').read_text()
    test = project / 'test.c'
    test.write_text('''#ifndef NXDK
#include <time.h>
#endif
#include "libssh2.h"
libssh2_struct_stat *s;
static ssize_t f(char *p, size_t len) { return (ssize_t)len; }
int main(void) { return s == 0 && f(0, 2) == 2 ? 0 : 1; }
''')
    for flags in ([], ['-DNXDK=1'], ['-DNXDK=1', '-D_MSC_VER=1930', '-D_INTEGRAL_MAX_BITS=64']):
        subprocess.run(['clang', '-fsyntax-only', '-std=c11', *flags, '-I', str(root / 'include'), str(test)], check=True)
    bsdtest = project / 'bsdtest.c'
    bsdtest.write_text('''#include "chacha.h"
#include "poly1305.h"
_Static_assert(sizeof(u_int) == 4, "u_int must be 32-bit");
_Static_assert(sizeof(u_char) == 1, "u_char must be 8-bit");
static struct chacha_ctx x;
int main(void) { return sizeof(x.input) == 64 ? 0 : 1; }
''')
    for extra in ([], ['-D_MSC_VER=1930']):
        subprocess.run(['clang', '-fsyntax-only', '-std=c11', '-DNXDK=1',
                        *extra, '-I', str(project / 'include'),
                        '-I', str(root / 'src'), str(bsdtest)], check=True)
    print('PASS: libssh2 public header and ChaCha/Poly1305 nxdk types compile, idempotence, config and socket patch')
