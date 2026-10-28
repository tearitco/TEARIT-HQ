#!/bin/sh
# build_book_check.sh - builds book_check into +x/ (read-only BOOK:PAGE conformance check of a pc-hq game folder against 18.pc-hq/PAGE-FILE.md).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/book_check.+x book_check.c
echo "OK +x/book_check.+x"
