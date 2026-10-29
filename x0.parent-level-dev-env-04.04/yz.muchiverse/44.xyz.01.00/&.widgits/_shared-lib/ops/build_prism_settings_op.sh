#!/bin/sh
# build_prism_settings_op.sh - builds prism_settings_op and its sibling prism_op into +x/ (steps one Custom-theme variable and re-renders the prism).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/prism_settings_op.+x prism_settings_op.c
[ -x +x/prism_op.+x ] || sh build_prism_op.sh
echo "OK +x/prism_settings_op.+x"
