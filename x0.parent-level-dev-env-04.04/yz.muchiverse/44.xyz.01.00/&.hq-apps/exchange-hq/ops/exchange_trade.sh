#!/bin/sh
# <cli_io> action: $1=pkg $2=house $3="<buy|sell> <unit> <amount_mc> [price_bp] [actor=name]". Pets/tests use the same file contract as the window.
PKG="$1"; RAW="$3"
[ -z "$RAW" ] && exit 0
SEQF="$PKG/exchange.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=TRADE:%s\n' "$N" "$RAW" > "$PKG/exchange_action.txt"
