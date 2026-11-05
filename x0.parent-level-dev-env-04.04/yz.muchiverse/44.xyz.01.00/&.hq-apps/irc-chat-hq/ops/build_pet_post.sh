#!/bin/sh
# build_pet_post.sh - compile the pet post guard (run behind cpu_gate like every compile).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=gnu11 -Wall -Wextra -O2 -o "$HERE/+x/pet_post.+x" "$HERE/pet_post.c"
echo "OK $HERE/+x/pet_post.+x"
