#!/bin/sh
# <item action="'tomom_item.sh' 'VERB ARG'"> : $1="VERB ARG" $2=pkg $3=house
LINE="$1"; PKG="$2"
case "$LINE" in
  "TAB "*)  CMD="TAB:${LINE#TAB }" ;;
  "SEL "*)  CMD="SEL:${LINE#SEL }" ;;
  UP|DOWN|BACK|RESET|UNDO|INIT|RELOAD|FLTCLR) CMD="$LINE" ;;
  PGNEXT)   CMD="PAGE+" ;;
  PGPREV)   CMD="PAGE-" ;;
  *) exit 0 ;;
esac
SEQF="$PKG/tomom.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/tomom_action.txt"
