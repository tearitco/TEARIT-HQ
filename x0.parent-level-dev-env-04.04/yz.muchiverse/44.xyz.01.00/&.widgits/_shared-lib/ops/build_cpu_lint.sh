#!/bin/sh
# build_cpu_lint.sh - build ops/+x/cpu_lint.+x (the unregistered-launch ratchet). 2026-11-02.
set -e
cd "$(dirname "$0")"
mkdir -p +x
echo "-- cpu_lint -> +x/cpu_lint.+x"
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/cpu_lint.+x cpu_lint.c
echo "OK +x/cpu_lint.+x"
