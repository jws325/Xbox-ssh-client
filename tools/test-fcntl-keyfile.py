#!/usr/bin/env python3
"""Regression tests for libssh2 FIONBIO portability and keyfile API config."""
from pathlib import Path
import importlib.util
import subprocess
import tempfile

project_root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('xssh_patch', project_root / 'tools' / 'patch-libssh2-nxdk.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as td:
    project = Path(td)
    src = project / 'third_party' / 'libssh2' / 'src'
    src.mkdir(parents=True)
    cfg = project / 'include' / 'mbedtls_config_xbox.h'
    cfg.parent.mkdir(parents=True)
    cfg.write_text('#define MBEDTLS_PK_PARSE_C\n#define MBEDTLS_PLATFORM_C\n')
    sess = src / 'session.c'
    sess.write_text('''#include <fcntl.h>
#include <lwip/sockets.h>
static int
session_nonblock(libssh2_socket_t sockfd,   /* operate on this */
                 int nonblock /* TRUE or FALSE */ )
{
#ifdef HAVE_O_NONBLOCK
    return fcntl(sockfd, F_SETFL, O_NONBLOCK);
#else
    (void)sockfd; (void)nonblock;
    return 0;
#endif
}
static int
get_socket_nonblocking(libssh2_socket_t sockfd)
{                               /* operate on this */
#ifdef HAVE_O_NONBLOCK
    return fcntl(sockfd, F_GETFL, 0) & O_NONBLOCK;
#else
    (void)sockfd;
    return 1;
#endif
}
int test_socket(libssh2_socket_t sock) {
    if(get_socket_nonblocking(sock) != 0) return 1;
    if(session_nonblock(sock, 1)) return 2;
    if(session_nonblock(sock, 0)) return 3;
    return 0;
}
''')
    module.patch_session_socket_mode(project / 'third_party' / 'libssh2')
    module.patch_mbedtls_file_io(project)
    original_src = sess.read_bytes()
    original_cfg = cfg.read_bytes()
    module.patch_session_socket_mode(project / 'third_party' / 'libssh2')
    module.patch_mbedtls_file_io(project)
    assert original_src == sess.read_bytes(), 'patch not idempotent'
    assert original_cfg == cfg.read_bytes(), 'config patch not idempotent'
    assert '#if !defined(NXDK)\n#include <fcntl.h>\n#endif' in sess.read_text()
    assert '#define MBEDTLS_FS_IO' in cfg.read_text()
    lwip = project / 'include' / 'lwip' / 'sockets.h'
    lwip.parent.mkdir()
    lwip.write_text('''#ifndef LWIP_SOCKETS_STUB
#define LWIP_SOCKETS_STUB
typedef int libssh2_socket_t;
#define FIONBIO 1234UL
extern int ioctlsocket(int fd, unsigned long cmd, unsigned long *mode);
#endif
''')
    probe = project / 'probe.c'
    probe.write_text('''static int num_calls;
static unsigned long last_mode;
int ioctlsocket(int fd, unsigned long cmd, unsigned long *mode) {
    if (fd != 7 || cmd != 1234UL) return -1;
    num_calls++;
    last_mode = *mode;
    if (num_calls == 1 && last_mode != 1) return -2;
    if (num_calls == 2 && last_mode != 0) return -3;
    return 0;
}
int test_socket(int);
int main(void) { return test_socket(7) || num_calls != 2 || last_mode != 0; }
''')
    exe = project / 'check'
    subprocess.run(['cc','-std=c11','-DNXDK=1','-I',str(project/'include'), str(sess), str(probe), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

assert '#define MBEDTLS_FS_IO' in (project_root/'include'/'mbedtls_config_xbox.h').read_text()
print('PASS: NXDK session uses ioctlsocket(FIONBIO), skips fcntl.h, enables FS_IO, patch idempotent')
