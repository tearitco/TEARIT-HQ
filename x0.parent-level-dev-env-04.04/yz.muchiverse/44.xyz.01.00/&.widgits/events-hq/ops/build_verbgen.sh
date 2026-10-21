#!/bin/sh
# build_verbgen.sh - compile verbgen.c (verb pages, BOOK 16.game/EVENT-VERB-PAGES-DESIGN.md) to ops/+x/verbgen.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -o +x/verbgen.+x verbgen.c && echo "OK $(pwd)/+x/verbgen.+x"
