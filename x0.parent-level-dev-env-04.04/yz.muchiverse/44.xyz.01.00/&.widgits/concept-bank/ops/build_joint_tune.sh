#!/bin/sh
# build_joint_tune.sh - compile joint_tune (self-contained C). -Wno-unknown-warning-option first so clang on macOS ignores gcc-only flags.
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -O2 -o +x/joint_tune.+x joint_tune.c
echo "OK +x/joint_tune.+x"
