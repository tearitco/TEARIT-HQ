#!/bin/sh
# build_chem_label.sh - compile chem_label (self-contained C).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -O2 -o +x/chem_label.+x chem_label.c
echo "OK +x/chem_label.+x"
