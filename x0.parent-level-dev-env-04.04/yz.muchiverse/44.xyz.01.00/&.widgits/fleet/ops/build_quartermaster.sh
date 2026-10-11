#!/bin/sh
# build_quartermaster.sh - compile quartermaster (self-contained C, no shared headers, no subprocesses). Output +x/quartermaster.+x (git-ignored).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
$CC -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/quartermaster.+x quartermaster.c
echo "OK +x/quartermaster.+x"
