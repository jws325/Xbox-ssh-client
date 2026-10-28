#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Iinclude src/network_config.c tools/test-network.c -o /tmp/xssh-network-test
/tmp/xssh-network-test
cc -std=c11 -Wall -Wextra -Werror -Itools/test-stubs -Iinclude \
  src/network_config.c src/network_xbox.c tools/test-network-xbox.c -o /tmp/xssh-network-xbox-test
/tmp/xssh-network-xbox-test
