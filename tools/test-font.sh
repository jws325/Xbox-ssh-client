#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  src/terminal.c src/font_size.c tools/test-font.c -o /tmp/xssh-font-test
/tmp/xssh-font-test

python3 tools/check-font-atlas.py

cc -std=c11 -Wall -Wextra -Werror -Iinclude src/font_style.c tools/test-font-style.c -o /tmp/xssh-font-style-test
/tmp/xssh-font-style-test
