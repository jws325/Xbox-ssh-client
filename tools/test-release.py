#!/usr/bin/env python3
"""Run XSSH's real release Make rules against a stubbed nxdk XBE generator."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
import re
build=(root/'include/xssh_build.h').read_text()
def macro(name):return re.search(r'^#define '+name+r' "([^"]+)"',build,re.M).group(1).encode()
markers = b'\0'.join([macro('XSSH_UI_LABEL'),macro('XSSH_BUILD_ID'),b'CONNECT TO SSH SERVER',b'SERVER IP',b'USERNAME',b'PASSWORD',b'xssh-known-hosts.txt']) 
v1 = b'XBEH-mock-v1' + markers + b'X' * 5000
v2 = b'XBEH-mock-v2' + markers + b'X' * 5000
assert '#define SEED_FILE "D:\\\\xssh-seed.bin"' in (root / 'src/rng.c').read_text()
assert 'nxMountDrive' not in (root / 'src/main.c').read_text()
assert 'release/' in (root / '.gitignore').read_text()

with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp) / 'xssh'
    work.mkdir()
    shutil.copy2(root / 'Makefile', work / 'Makefile')
    (work / 'include').mkdir()
    shutil.copy2(root / 'include/xssh_build.h', work / 'include/xssh_build.h')
    shutil.copy2(root / 'xssh.cfg', work / 'xssh.cfg')
    (work / 'tools').mkdir()
    shutil.copy2(root / 'tools/verify-built-xbe.py', work / 'tools/verify-built-xbe.py')
    shutil.copy2(root / 'tools/make-seed.py', work / 'tools/make-seed.py')
    shutil.copy2(root / 'tools/create-demo-saver.py', work / 'tools/create-demo-saver.py')
    sdk = Path(tmp) / 'nxdk'
    sdk.mkdir()
    (sdk / 'Makefile').write_text('''
OUTPUT_DIR = bin
TARGET = $(OUTPUT_DIR)/default.xbe
all: $(TARGET)
$(TARGET):
\t@mkdir -p $(OUTPUT_DIR)
\t@cp $(CURDIR)/tools/mock-v1.xbe $@
''')
    (work / 'tools/mock-v1.xbe').write_bytes(v1)
    def run_make(*args, should_succeed=True):
        completed = subprocess.run(['make', '--no-print-directory', f'NXDK_DIR={sdk}', *args], cwd=work, capture_output=True, text=True)
        assert (completed.returncode == 0) == should_succeed, completed.stdout + completed.stderr
        return completed
    run_make('all', '-j4')
    rel=work/'release'
    assert sorted(x.name for x in rel.iterdir()) == ['default.xbe','screensaver.xsv','xssh-seed.bin','xssh.cfg']
    screensaver=(rel/'screensaver.xsv').read_bytes()
    assert screensaver[:8]==b'XSSHRGB5'
    seed=(rel/'xssh-seed.bin').read_bytes()
    assert len(seed)==64 and seed!=bytes(64)
    assert (rel/'default.xbe').read_bytes()==v1
    assert macro('XSSH_BUILD_ID') in v1
    assert (rel/'xssh.cfg').read_bytes() == (work/'xssh.cfg').read_bytes()
    (work/'xssh.cfg').write_text('host=192.168.2.50\n')
    (work/'bin/default.xbe').write_bytes(v2)
    run_make('all')
    assert (rel/'xssh-seed.bin').read_bytes()==seed, 'incremental build changed RNG state'
    assert (rel/'screensaver.xsv').read_bytes()==screensaver, 'incremental build changed video'
    assert (rel/'default.xbe').read_bytes()==v2
    assert (rel/'xssh.cfg').read_text()=='host=192.168.2.50\n'
    # A stale binary without the interactive form must be rejected.
    (work/'bin/default.xbe').write_bytes(b'OLD-LOGINLESS-XBE' + b'X'*6000)
    run_make('release', should_succeed=False)
    (work/'bin/default.xbe').write_bytes(v2)
    # Existing malformed seeds must be treated as fatal, not replaced.
    (rel/'xssh-seed.bin').write_bytes(b'bad')
    run_make('release', should_succeed=False)
    assert (rel/'xssh-seed.bin').read_bytes()==b'bad'
    (rel/'xssh-seed.bin').unlink()
    run_make('release')
    assert len((rel/'xssh-seed.bin').read_bytes())==64
    assert (rel/'xssh-seed.bin').read_bytes() != seed
    # The seed generator must never overwrite a seed file directly.
    a = subprocess.run(['python3','tools/make-seed.py',str(rel/'xssh-seed.bin')], cwd=work, capture_output=True, text=True)
    assert a.returncode != 0
print('PASS: automatic release creation, 4 files, crypto seed generation, incremental preservation, invalid-seed detection')
