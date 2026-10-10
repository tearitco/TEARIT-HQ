#!/bin/sh
# <cli_io> action: $1=pkg $2=house $3="<LIST|BID|BUY|CANCEL> args". Actor = AUCTION_ACTOR env, else the login name. Pets/tests use the same file contract as the window.
PKG="$1"; RAW="$3"
[ -z "$RAW" ] && exit 0
ACTOR="${AUCTION_ACTOR:-${USER:-human}}"
SEQF="$PKG/auction.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=AUC:%s|%s\n' "$N" "$ACTOR" "$RAW" > "$PKG/auction_action.txt"
