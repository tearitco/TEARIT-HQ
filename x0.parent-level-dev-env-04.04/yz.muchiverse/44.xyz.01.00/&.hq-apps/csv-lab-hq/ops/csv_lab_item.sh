#!/bin/sh
# <item action="'csv_lab_item.sh' 'VERB ARG'"> : $1="VERB ARG" $2=pkg $3=house
# <cli_io action="'csv_lab_item.sh'">          : $1=pkg $2=house $3=the live typed text (renderer's cli_io convention)
# Turns a click / typed line into seq=/cmd= for csv_lab_manager.+x's poll (csv_lab_action.txt).
# The manager is the only writer of the review and command files; this script writes none of them.
if [ -d "$1" ] && [ -n "$3" ]; then            # cli_io call: pkg house text
    PKG="$1"; TEXT="$(printf '%s' "$3" | tr '\r\n\t' '   ')"; CMD="CMD:$TEXT"
else
    LINE="$1"; PKG="$2"
    case "$LINE" in
      "SEL "*) CMD="SEL:${LINE#SEL }" ;;
      "BANK "*) CMD="BANK:${LINE#BANK }" ;;
      "FILTER "*) CMD="FILTER:${LINE#FILTER }" ;;
      ACCEPT|REJECT|RELOAD|NOOP) CMD="$LINE" ;;
      *) exit 0 ;;
    esac
fi
[ -n "$PKG" ] || exit 0
SEQF="$PKG/csv_lab.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/csv_lab_action.txt"
