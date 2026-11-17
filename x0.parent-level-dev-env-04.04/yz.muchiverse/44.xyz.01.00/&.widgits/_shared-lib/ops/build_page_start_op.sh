#!/bin/sh
# build_page_start_op.sh - build the shared page-start op (page_start_op.c; centres the editor cursor on a painted page; used by the phone editor and the desktop editor launcher)
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/page_start_op.+x page_start_op.c && echo "OK +x/page_start_op.+x"
