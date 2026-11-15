#!/bin/sh
# builds the Tools list op (harness_menu_op): the one authority for the rows of the editor Tools tab, the LyfeDesk Tools cell and the plugz-hq windows (D74, D76). Missing before 2026-11-14: it had been built by hand.
cd "$(dirname "$0")" && mkdir -p +x && echo "-- harness_menu_op -> +x/harness_menu_op.+x" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/harness_menu_op.+x harness_menu_op.c && echo "OK +x/harness_menu_op.+x"
