#!/usr/bin/env python3
"""Create a secret initial state for Xbox crypto; copy to E:\\xssh-seed.bin.
This is NOT safe to place on a DVD or other read-only filesystem.
"""
import secrets
from pathlib import Path
import sys
p = Path(sys.argv[1] if len(sys.argv) > 1 else 'xssh-seed.bin')
if p.exists():
    raise SystemExit(f'{p} exists: refusing to overwrite secret state')
with p.open('xb') as f:
    f.write(secrets.token_bytes(64))
print(f'Wrote 64-byte secret to {p}. Transfer to E:\\xssh-seed.bin and restrict access.')
