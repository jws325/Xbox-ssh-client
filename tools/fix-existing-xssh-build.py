#!/usr/bin/env python3
"""Compatibility entrypoint retained for older XSSH instructions.

Patches the currently fetched libssh2 headers and XSSH configuration for
nxdk's non-POSIX libc. Run this from the XSSH project directory.
"""
from pathlib import Path
import runpy
import sys

script = Path(__file__).resolve().parent / 'patch-libssh2-nxdk.py'
if len(sys.argv) == 1:
    sys.argv.append(str(Path.cwd()))
runpy.run_path(str(script), run_name='__main__')
