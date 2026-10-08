#!/bin/sh
# build_entity_grade.sh - compile entity_grade (self-contained C).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -O2 -o +x/entity_grade.+x entity_grade.c
echo "OK +x/entity_grade.+x"
