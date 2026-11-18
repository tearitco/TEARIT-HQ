#!/bin/sh
# build_book_path_op.sh - build the book locator op (book_path_op.c; shell front of ../pchq_book_path.c, the one place that finds a book folder)
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/book_path_op.+x book_path_op.c && echo "OK +x/book_path_op.+x"
