#!/bin/sh
# <cli_io id="askbox"> action: $1=pkg $2=house $3=typed prompt
PKG="$1"; TXT=$(printf '%s' "$3" | tr '\r\n\t' '   ')
[ -z "$TXT" ] && exit 0
SEQF="$PKG/tomom.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=ASK:%s\n' "$N" "$TXT" > "$PKG/tomom_action.txt"
