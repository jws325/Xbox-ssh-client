#!/usr/bin/env python3
"""Run XSSH's real release Make rules against a stubbed nxdk XBE generator."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
assert '#define SEED_FILE "D:\\\\xssh-seed.bin"' in (root / 'src/rng.c').read_text()
assert 'nxMountDrive' not in (root / 'src/main.c').read_text()
assert 'release/' in (root / '.gitignore').read_text()

with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp) / 'xssh'
    work.mkdir()
    shutil.copy2(root / 'Makefile', work / 'Makefile')
    shutil.copy2(root / 'xssh.cfg', work / 'xssh.cfg')
    (work / 'tools').mkdir()
    shutil.copy2(root / 'tools/make-seed.py', work / 'tools/make-seed.py')
    sdk = Path(tmp) / 'nxdk'
    sdk.mkdir()
    (sdk / 'Makefile').write_text('''
OUTPUT_DIR = bin
TARGET = $(OUTPUT_DIR)/default.xbe
all: $(TARGET)
$(TARGET):
\t@mkdir -p $(OUTPUT_DIR)
\t@printf 'XBE-mock-v1' > $@
''')
    def run_make(*args, should_succeed=True):
        completed = subprocess.run(['make', '--no-print-directory', f'NXDK_DIR={sdk}', *args], cwd=work, capture_output=True, text=True)
        assert (completed.returncode == 0) == should_succeed, completed.stdout + completed.stderr
        return completed
    run_make('all', '-j4')
    rel=work/'release'
    assert sorted(x.name for x in rel.iterdir()) == ['default.xbe','xssh-seed.bin','xssh.cfg']
    seed=(rel/'xssh-seed.bin').read_bytes()
    assert len(seed)==64 and seed!=bytes(64)
    assert (rel/'default.xbe').read_bytes()==b'XBE-mock-v1'
    assert (rel/'xssh.cfg').read_bytes() == (work/'xssh.cfg').read_bytes()
    (work/'xssh.cfg').write_text('host=192.168.2.50\n')
    (work/'bin/default.xbe').write_bytes(b'XBE-mock-v2')
    run_make('all')
    assert (rel/'xssh-seed.bin').read_bytes()==seed, 'incremental build changed RNG state'
    assert (rel/'default.xbe').read_bytes()==b'XBE-mock-v2'
    assert (rel/'xssh.cfg').read_text()=='host=192.168.2.50\n'
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
print('PASS: automatic release creation, 3 files, crypto seed generation, incremental preservation, invalid-seed detection')
