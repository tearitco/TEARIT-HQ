#!/bin/sh
# build_hidden_layer.sh - compile hidden_layer (self-contained C).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -O2 -o +x/hidden_layer.+x hidden_layer.c
echo "OK +x/hidden_layer.+x"
