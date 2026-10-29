#!/usr/bin/env python3
"""Preflight guard: all nxdk compilation units must see NXDK=1.

This test does not compile against the Xbox SDK.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

project = Path(__file__).resolve().parent.parent
makefile = (project / "Makefile").read_text()
patcher = (project / "tools" / "patch-libssh2-nxdk.py").read_text()
assert re.search(r"(?m)^NXDK_CFLAGS\s*\+=\s*-DNXDK=1\b", makefile), "Missing -DNXDK=1 in nxdk CFLAGS"
assert "MSVC_TYPES_PATCH" in patcher and "!defined(NXDK)" in patcher
assert "MSVC_FILES_PATCH" in patcher
assert "content = replace_one(content, MSVC_TYPES_ORIGINAL, MSVC_TYPES_PATCH, inc)" in patcher
assert "content = replace_one(content, MSVC_FILES_ORIGINAL, MSVC_FILES_PATCH, inc)" in patcher

# Check what make actually passes after including a dummy SDK Makefile.
if shutil.which("make"):
    with tempfile.TemporaryDirectory() as folder:
        fake = Path(folder) / "Makefile"
        fake.write_text("\ntest-nxdk-flags:\n\t@printf '%s\\n' '$(NXDK_CFLAGS)'\n")
        result = subprocess.run(["make", "--no-print-directory", "-f", str(project / "Makefile"),
                                 "NXDK_DIR=" + folder, "test-nxdk-flags"],
                                cwd=project, text=True, capture_output=True, check=True)
        assert "-DNXDK=1" in result.stdout, result.stdout
print("PASS: NXDK=1 is passed to all nxdk C compile units; guarded libssh2 patch present")
