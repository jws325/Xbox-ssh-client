#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
"$CC" -std=c11 -Wall -Wextra -Werror -Iinclude src/hid_keys.c tools/test-hid-keys.c -o /tmp/xssh-hid-test
/tmp/xssh-hid-test
"$CC" -std=c11 -Wall -Wextra -Werror -Itools/test-stubs -Iinclude src/hid_keys.c src/usb_keyboard.c tools/test-usb-keyboard.c -o /tmp/xssh-usb-test
/tmp/xssh-usb-test
"$CC" -std=c11 -Wall -Wextra -Werror -Iinclude src/terminal.c tools/test-terminal.c -o /tmp/xssh-term-test
/tmp/xssh-term-test
