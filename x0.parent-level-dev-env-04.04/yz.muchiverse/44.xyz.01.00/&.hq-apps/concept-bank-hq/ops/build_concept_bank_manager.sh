#!/bin/sh
# build_concept_bank_manager.sh - compile the concept-bank-hq window backend (self-contained .c).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -O2 \
    -o "$HERE/+x/concept_bank_manager.+x" "$HERE/concept_bank_manager.c" -lm
echo "OK $HERE/+x/concept_bank_manager.+x"
