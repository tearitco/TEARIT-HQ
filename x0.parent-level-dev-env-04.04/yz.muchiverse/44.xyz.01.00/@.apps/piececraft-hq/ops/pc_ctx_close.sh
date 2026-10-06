#!/bin/sh
# pc_ctx_close.sh <state_dir> - hide the in-board context menu (chrome x / _): ctx_visible=0, board reparse drops it.
printf 'ctx_visible=0\n' > "$1/ctx.txt"
