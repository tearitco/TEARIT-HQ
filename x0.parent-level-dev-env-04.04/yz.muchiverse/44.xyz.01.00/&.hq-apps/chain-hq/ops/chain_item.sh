#!/bin/sh
# <item action="'chain_item.sh' 'VERB ARG'"> : $1="VERB ARG" $2=pkg $3=house
LINE="$1"; PKG="$2"
case "$LINE" in
  "TAB "*)     CMD="TAB:${LINE#TAB }" ;;
  MINE_TOGGLE) CMD="MINE_TOGGLE" ;;
  REFRESH)     CMD="REFRESH" ;;
  *) exit 0 ;;
esac
SEQF="$PKG/chain.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/chain_action.txt"
