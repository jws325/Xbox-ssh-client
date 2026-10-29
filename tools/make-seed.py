#!/usr/bin/env python3
"""Create a unique 64-byte seed for the writable Xbox XSSH release folder.

The build calls this only if release/xssh-seed.bin does not already exist.
This secret MUST NOT be put in public archives, reused on several consoles,
or overwritten when updating the executable on an already installed Xbox.
"""
import os
from pathlib import Path
import secrets
import sys


def create_seed(destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive create; never silently reset an existing deployed secret.
    fd = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, 'wb') as out:
            out.write(secrets.token_bytes(64))
    except BaseException:
        destination.unlink(missing_ok=True)
        raise
    print(f'Generated unique 64-byte XSSH secret: {destination}')


if __name__ == '__main__':
    path = Path(sys.argv[1] if len(sys.argv) > 1 else 'release/xssh-seed.bin')
    try:
        create_seed(path)
    except FileExistsError:
        raise SystemExit(f'Existing secret not overwritten: {path}')
