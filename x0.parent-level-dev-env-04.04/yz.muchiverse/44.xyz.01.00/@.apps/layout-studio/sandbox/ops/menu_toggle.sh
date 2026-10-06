#!/bin/sh
# Flip the sandbox test menu's visibility (state/menu.txt tm_visible); the board's live reparse drops/re-adds it.
F="$(cd "$(dirname "$0")/.." && pwd)/state/menu.txt"
case "$(sed -n 's/^tm_visible=//p' "$F" 2>/dev/null)" in 0) v=1 ;; *) v=0 ;; esac
printf 'tm_visible=%s\n' "$v" > "$F"
