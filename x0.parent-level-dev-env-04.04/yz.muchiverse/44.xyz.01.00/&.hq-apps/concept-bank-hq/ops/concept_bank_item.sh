#!/bin/sh
# <item action="'concept_bank_item.sh' 'VERB ARG'"> : $1="VERB ARG" $2=pkg $3=house
LINE="$1"; PKG="$2"
case "$LINE" in
  "TAB "*) CMD="TAB:${LINE#TAB }" ;;
  "SEL "*) CMD="SEL:${LINE#SEL }" ;;
  BACK|RELOAD|VALIDATE) CMD="$LINE" ;;
  *) exit 0 ;;
esac
SEQF="$PKG/concept_bank.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/concept_bank_action.txt"
