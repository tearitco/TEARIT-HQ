#!/bin/sh
# build_pet_gen.sh - builds ops/+x/pet_gen.+x (run from @.apps/layout-studio)
cd "$(dirname "$0")/.." || exit 1
mkdir -p ops/+x
gcc -std=c11 -O2 -Wall -Wextra -Wno-unused-result -o ops/+x/pet_gen.+x ops/pet_gen.c -lm
