#!/usr/bin/env python3
"""Fix libssh2/Mbed TLS RSA build config for XSSH on nxdk.

Run from the project root or pass the project directory as an argument.
Idempotent; does not modify downloaded third-party sources.
"""
from pathlib import Path
import re
import sys


def fix(root: Path) -> bool:
    path = root / "include" / "mbedtls_config_xbox.h"
    if not path.is_file():
        raise FileNotFoundError(f"XSSH Mbed TLS configuration not found: {path}")
    original = path.read_text(encoding="utf-8")
    rsa = re.search(r"(?m)^\s*#\s*define\s+MBEDTLS_RSA_C\b", original)
    if not rsa:
        raise RuntimeError(f"MBEDTLS_RSA_C not enabled in {path}")
    if re.search(r"(?m)^\s*#\s*define\s+MBEDTLS_PKCS1_V(?:15|21)\b", original):
        print("Mbed TLS already has a PKCS#1 RSA scheme enabled; no changes needed.")
        return False
    patched = original[:rsa.start()] + ("/* RSA SSH signatures require PKCS#1 v1.5 with SHA-2. */\n"
               "#define MBEDTLS_PKCS1_V15\n") + original[rsa.start():]
    path.write_text(patched, encoding="utf-8")
    print(f"Enabled MBEDTLS_PKCS1_V15 in {path}")
    return True


if __name__ == "__main__":
    project = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
    try:
        fix(project.resolve())
    except (FileNotFoundError, RuntimeError) as exc:
        sys.exit(str(exc))
