#!/bin/sh
# build_book_page_op.sh - builds book_page_op into +x/ (create/list/rename book:pages under a sessions root; verify: the pal harness harness/book_page.pal).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=c11 -Wall -Wextra -Wno-format-truncation -O2 -o +x/book_page_op.+x book_page_op.c
echo "OK +x/book_page_op.+x"
