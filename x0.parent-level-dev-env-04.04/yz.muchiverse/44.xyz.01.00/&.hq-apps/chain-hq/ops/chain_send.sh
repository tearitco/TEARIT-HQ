#!/bin/sh
# <cli_io id="sendline"> action: $1=pkg $2=house $3="<to> <amount>"
PKG="$1"; RAW="$3"
[ -z "$RAW" ] && exit 0
TO=$(printf '%s' "$RAW" | awk '{print $1}')
AMT=$(printf '%s' "$RAW" | awk '{print $2}')
[ -z "$TO" ] || [ -z "$AMT" ] && exit 0
SEQF="$PKG/chain.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=SEND:%s|%s\n' "$N" "$TO" "$AMT" > "$PKG/chain_action.txt"
