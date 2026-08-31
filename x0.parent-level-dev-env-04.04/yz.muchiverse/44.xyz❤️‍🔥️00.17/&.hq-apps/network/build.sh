#!/bin/bash
# build.sh - builds the cli-io window binary (network cell Browser stub).
# Binaries live in +x/ (git-ignored). Core X only, no Xft dependency.
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x"
gcc -O2 -Wall -Wno-unused-result -o "$SDIR/+x/cli_io_window.+x" "$SDIR/cli_io_window.c" -lX11 && echo "OK cli_io_window" || exit 1