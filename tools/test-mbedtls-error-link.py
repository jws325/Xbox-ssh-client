#!/usr/bin/env python3
"""Test XSSH Mbed TLS error.c link dependency configuration + fix script."""
from pathlib import Path
import importlib.util
import re
import tempfile

root = Path(__file__).resolve().parent.parent
script = root / 'tools' / 'fix-mbedtls-strerror.py'
spec = importlib.util.spec_from_file_location('fix_mbedtls_strerror', script)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

mk = (root / 'Makefile').read_text()
cfg = (root / 'include' / 'mbedtls_config_xbox.h').read_text()
match = re.search(r'(?ms)^TLS_C\s*=\s*(.*?)(?=^\S[^\n]*\s*=|^#|^include\s|\Z)', mk)
assert match, 'TLS_C missing'
assert re.search(r'\berror\b', match.group(1)), 'Mbed TLS error.c missing from source list'
assert '#define MBEDTLS_ERROR_C\n' in cfg, 'Mbed TLS error implementation disabled'

# Verify independently against a small older Makefile / crypto config fixture.
with tempfile.TemporaryDirectory() as d:
    proj = Path(d)
    conf = proj / 'include' / 'mbedtls_config_xbox.h'
    conf.parent.mkdir()
    conf.write_text('#define MBEDTLS_ENTROPY_C\n')
    make = proj / 'Makefile'
    make.write_text('TLS_C = aes entropy entropy_poll md sha256\n\nSRCS = app.c\n')
    assert mod.fix(proj)
    after_make, after_cfg = make.read_text(), conf.read_text()
    assert 'entropy_poll error md' in after_make
    assert '#define MBEDTLS_ERROR_C' in after_cfg
    assert not mod.fix(proj), 'Should be idempotent'
    assert (make.read_text(), conf.read_text()) == (after_make, after_cfg)
print('PASS: mbedtls_strerror provider source and configuration enabled, patch idempotent')
