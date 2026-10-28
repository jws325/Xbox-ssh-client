#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Itools/test-stubs -Iinclude \
    src/video_modes.c src/video_xbox.c tools/test-video.c -o /tmp/xssh-video-test
/tmp/xssh-video-test
