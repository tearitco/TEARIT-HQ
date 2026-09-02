#!/bin/sh
# house style: self-contained op, clean + build + test
set -e
cd "$(dirname "$0")"
mkdir -p +x
gcc -std=gnu11 -O2 -Wall -Wextra -Isrc src/scm_core.h src/scm_load.c src/scm_features.c src/scm_select.c src/scm_cli.c -lm -o +x/scm_cli
+x/scm_cli list
