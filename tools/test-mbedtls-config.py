#!/usr/bin/env python3
"""Native regression checks for XSSH's Mbed TLS RSA option."""
from pathlib import Path
import importlib.util
import re
import tempfile

root = Path(__file__).resolve().parent.parent
script = root / "tools" / "fix-mbedtls-rsa-config.py"
spec = importlib.util.spec_from_file_location("fix_mbedtls_rsa_config", script)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

config = (root / "include" / "mbedtls_config_xbox.h").read_text(encoding="utf-8")
for option in ("MBEDTLS_RSA_C", "MBEDTLS_PKCS1_V15"):
    assert re.search(rf"(?m)^\s*#\s*define\s+{option}\b", config), option

with tempfile.TemporaryDirectory() as dirname:
    testroot = Path(dirname)
    path = testroot / "include" / "mbedtls_config_xbox.h"
    path.parent.mkdir()
    path.write_text("#define MBEDTLS_RSA_C\n", encoding="utf-8")
    assert mod.fix(testroot)
    contents = path.read_text(encoding="utf-8")
    assert "#define MBEDTLS_PKCS1_V15\n" in contents
    assert not mod.fix(testroot)
    assert contents == path.read_text(encoding="utf-8")
print("PASS: Mbed TLS RSA PKCS#1 v1.5 enabled; standalone fix is idempotent")

# Mbed TLS 3.6.5 platform_util.h includes <sal.h> on MSVC targets unless
# MBEDTLS_CHECK_RETURN is pre-defined. nxdk does not ship Microsoft's SAL SDK.
import subprocess
import shutil
assert re.search(r"(?m)^\s*#\s*define\s+MBEDTLS_CHECK_RETURN\s+__attribute__\(\(__warn_unused_result__\)\)", config)
with tempfile.TemporaryDirectory() as dirname:
    testroot = Path(dirname)
    path = testroot / "include" / "mbedtls_config_xbox.h"
    path.parent.mkdir()
    path.write_text("#define MBEDTLS_CONFIG_VERSION 0x03000000\n", encoding="utf-8")
    sal_script = root / "tools" / "fix-mbedtls-sal-config.py"
    spec = importlib.util.spec_from_file_location("fix_mbedtls_sal_config", sal_script)
    sal_mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sal_mod)
    assert sal_mod.fix(testroot)
    assert not sal_mod.fix(testroot)
    assert "#define MBEDTLS_CHECK_RETURN __attribute__((__warn_unused_result__))" in path.read_text()
    if shutil.which('cc'):
        source = '#include "mbedtls_config_xbox.h"\nMBEDTLS_CHECK_RETURN int check(void);\nint check(void){return 1;}\n'
        result = subprocess.run(['cc', '-x', 'c', '-fsyntax-only', '-D_MSC_VER=1930', '-I', str(path.parent), '-'], input=source, text=True, capture_output=True)
        assert result.returncode == 0, result.stderr
print("PASS: Mbed TLS SAL workaround enabled; standalone fix idempotent")
