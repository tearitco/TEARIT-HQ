#!/bin/sh
# Hotbar / tab / footer actions. Two call shapes: <item action="'exchange_item.sh' 'VERB ARG'"> gives $1="VERB ARG" $2=pkg $3=house;
# a <tab onclick="sh exchange_item.sh 'VERB ARG'"> gives only $1, so the package dir is derived from this script's own location.
LINE="$1"; PKG="${2:-$(cd "$(dirname "$0")/.." && pwd)}"
case "$LINE" in
  "TAB "*)   CMD="TAB:${LINE#TAB }" ;;
  "UNIT "*)  CMD="UNIT:${LINE#UNIT }" ;;
  "SET "*)   CMD="SET:${LINE#SET }" ;;
  "QUICK "*) CMD="QUICK:${LINE#QUICK }" ;;
  CHART)     CMD="CHART" ;;
  REFRESH)   CMD="REFRESH" ;;
  *) exit 0 ;;
esac
SEQF="$PKG/exchange.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/exchange_action.txt"
