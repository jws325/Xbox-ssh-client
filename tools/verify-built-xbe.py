#!/usr/bin/env python3
"""Reject stale XSSH executables before release packaging.

This validates XBE format, a release-specific identifier displayed by the
interactive login form, and critical login-screen strings. It does NOT prove
runtime behavior on an Xbox or that the build was made with a trusted SDK.
"""
import argparse
import hashlib
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
BUILD_HEADER = ROOT / "include" / "xssh_build.h"
FORM_MARKERS = (b"CONNECT TO SSH SERVER", b"SERVER IP", b"USERNAME",
                b"PASSWORD", b"xssh-known-hosts.txt")


def macro(name):
    content = BUILD_HEADER.read_text(encoding="utf-8")
    match = re.search(r'^\s*#define\s+' + re.escape(name) + r'\s+"([ -~]+)"', content, re.M)
    if not match:
        raise ValueError(f"Missing or invalid {name} in {BUILD_HEADER}")
    return match.group(1).encode("ascii")


def verify(path):
    if not path.is_file():
        print(f"ERROR: XBE not found: {path}", file=sys.stderr)
        return 1
    data = path.read_bytes()
    if len(data) < 4096 or not data.startswith(b"XBEH"):
        print("ERROR: File does not appear to be a complete Xbox XBE (XBEH magic/size)", file=sys.stderr)
        return 1
    try:
        build_id = macro("XSSH_BUILD_ID")
        ui_label = macro("XSSH_UI_LABEL")
    except (ValueError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    markers = (build_id, ui_label) + FORM_MARKERS
    missing = [marker.decode("ascii") for marker in markers if marker not in data]
    if missing:
        print("ERROR: XBE is missing interactive-login release markers:",
              ", ".join(missing), file=sys.stderr)
        print("Rebuild from this checkout; do not copy an older default.xbe.", file=sys.stderr)
        return 1
    print(f"PASS: release build ID: {build_id.decode()}")
    print(f"PASS: login label: {ui_label.decode()}")
    print(f"XBE: {path} ({len(data)} bytes)")
    print(f"SHA-256: {hashlib.sha256(data).hexdigest()}")
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xbe", type=Path, help="XBE to verify")
    raise SystemExit(verify(parser.parse_args().xbe))
