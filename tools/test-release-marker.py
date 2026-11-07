#!/usr/bin/env python3
"""Test XSSH release verification logic against controlled XBE-shaped files."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
import re
header=(root/"include/xssh_build.h").read_text()
def macro(name): return re.search(r'^#define '+name+r' "([^"]+)"',header,re.M).group(1).encode()
markers = [macro("XSSH_BUILD_ID"),macro("XSSH_UI_LABEL"),b"CONNECT TO SSH SERVER",
           b"SERVER IP",b"USERNAME",b"PASSWORD",b"xssh-known-hosts.txt"]

with tempfile.TemporaryDirectory() as t:
    file = Path(t) / "default.xbe"
    def verify(expected):
        result = subprocess.run(["python3", str(root / "tools/verify-built-xbe.py"), str(file)],
                                capture_output=True, text=True)
        assert (result.returncode == 0) == expected, result.stdout + result.stderr
    good = b"XBEH" + b"\x00"*256 + b"\x00".join(markers) + b"\x00"*5000
    file.write_bytes(good)
    verify(True)
    for missing in markers:
        file.write_bytes(good.replace(missing, b"REMOVED"))
        verify(False)
    file.write_bytes(b"MZ" + good[2:])
    verify(False)
    file.write_bytes(good[:100])
    verify(False)
print("PASS: XBE build ID and login markers verified, stale/corrupted XBE rejected")
