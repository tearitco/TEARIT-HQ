#!/bin/sh
# build_desk_restart.sh - builds desk_restart into +x/ (restart a house desktop without ever leaving it silently down; verify: pal harness _shared-lib/harness/desk_restart.pal).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -O2 -o +x/desk_restart.+x desk_restart.c
echo "OK +x/desk_restart.+x"
