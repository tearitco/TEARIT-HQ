#!/bin/sh
# <cli_io id="flt"> action: $1=pkg $2=house $3=typed filter text (empty = clear)
PKG="$1"; TXT=$(printf '%s' "$3" | tr '\r\n\t|' '    ')
SEQF="$PKG/tomom.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=FILTER:%s\n' "$N" "$TXT" > "$PKG/tomom_action.txt"
