#!/bin/bash
# active_corp.sh - resolves the CYCLE_CORP-tracked active_corp_index
# into an actual corp_<TICKER> piece_id, same directory-listing shape
# wsr_compose_frame.c's own get_nth_corp() already uses. Exists so
# piece.pdl METHOD rows (Trade/Management/Financing/Derivatives - all
# "act on whichever corp is selected") can resolve it via one shared
# shell command substitution instead of duplicating the lookup logic
# in every row's command string. Assumes CWD is already the project
# root (true for anything invoked via wsr_menu_input.c's RUN: prefix).
idx=$(grep '^active_corp_index=' projects/wsr-pal/pieces/wsr_menu/state.txt 2>/dev/null | cut -d= -f2)
idx=${idx:-0}
ls -d projects/wsr-pal/pieces/corp_*/ 2>/dev/null | sed -n "$((idx + 1))p" | xargs -n1 basename
