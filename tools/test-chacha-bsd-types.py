#!/usr/bin/env python3
"""Standalone ChaCha20/Poly1305 type compatibility regression test.
Requires only Python 3 and clang on the host machine.
"""
from pathlib import Path
import importlib.util
import subprocess
import tempfile

script = Path(__file__).parent / 'patch-libssh2-nxdk.py'
spec = importlib.util.spec_from_file_location('nxdk_patch', script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as d:
    project = Path(d)
    src = project / 'third_party' / 'libssh2' / 'src'
    src.mkdir(parents=True)
    (project / 'include').mkdir(parents=True)
    (src / 'chacha.h').write_text('''/* libssh2 1.11.1 vendor fragment */
#ifndef CHACHA_H
#define CHACHA_H
#include <stdlib.h>
struct chacha_ctx { u_int input[16]; };
void chacha_keysetup(struct chacha_ctx *, const u_char *, u_int);
void chacha_ivsetup(struct chacha_ctx *, const u_char *, const u_char *);
void chacha_encrypt_bytes(struct chacha_ctx *, const u_char *, u_char *, u_int);
#endif
''')
    (src / 'poly1305.h').write_text('''/* libssh2 1.11.1 vendor fragment */
#ifndef POLY1305_H
#define POLY1305_H
#define POLY1305_TAGLEN 16
#define POLY1305_KEYLEN 32
void poly1305_auth(u_char out[POLY1305_TAGLEN], const u_char *m, size_t inlen,
                   const u_char key[POLY1305_KEYLEN]);
#endif
''')
    (src / 'cipher-chachapoly.h').write_text('''#ifndef CHACHA_POLY_AEAD_H
#define CHACHA_POLY_AEAD_H
#include "chacha.h"
#include "poly1305.h"
struct chachapoly_ctx { struct chacha_ctx main_ctx, header_ctx; };
int chachapoly_init(struct chachapoly_ctx *, const u_char *, u_int);
#endif
''')
    module.patch_chacha_bsd_types(project / 'third_party' / 'libssh2')
    files = [src / 'chacha.h', src / 'poly1305.h', project / 'include' / 'xssh_nxdk_bsd_types.h']
    snapshot = {f:f.read_bytes() for f in files}
    module.patch_chacha_bsd_types(project / 'third_party' / 'libssh2')
    assert all(f.read_bytes() == snapshot[f] for f in files), 'patch is not idempotent'
    c = project / 'test.c'
    c.write_text('''#include "cipher-chachapoly.h"
_Static_assert(sizeof(u_char)==1, "u_char size");
_Static_assert(sizeof(u_int)==4, "u_int size");
_Static_assert(sizeof(((struct chacha_ctx*)0)->input)==64, "chacha state");
int main(void) { return 0; }
''')
    for macro in ([], ['-D_MSC_VER=1930']):
        subprocess.run(['clang', '-std=c11', '-Werror', '-fsyntax-only',
                        '-DNXDK=1', *macro,
                        '-I', str(src), '-I', str(project / 'include'), str(c)], check=True)
    print('PASS: nxdk ChaCha/Poly1305 BSD aliases and include order; repeat-safe')
