#!/bin/sh
# build_ledger_to_feedback.sh - compile ledger_to_feedback (self-contained C). -Wno-unknown-warning-option first so clang on macOS ignores gcc-only flags.
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -O2 -o +x/ledger_to_feedback.+x ledger_to_feedback.c
echo "OK +x/ledger_to_feedback.+x"
