#!/bin/sh
# build_quest_check.sh - compile quest_check (self-contained C, no shared headers; fork+exec only).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
$CC -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/quest_check.+x quest_check.c
echo "OK +x/quest_check.+x"
