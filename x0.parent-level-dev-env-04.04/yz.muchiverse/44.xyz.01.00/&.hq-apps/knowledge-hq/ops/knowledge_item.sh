#!/bin/sh
# <item action="'knowledge_item.sh' 'VERB ARG'"> : $1="VERB ARG" $2=pkg $3=house
# Turns a click into seq=/cmd= for knowledge_manager.+x's poll (knowledge_action.txt).
LINE="$1"; PKG="$2"
[ -n "$PKG" ] || exit 0
case "$LINE" in
  "SHOW "*) CMD="SHOW:${LINE#SHOW }" ;;
  "FILTER "*) CMD="FILTER:${LINE#FILTER }" ;;
  "SEL "*) CMD="SEL:${LINE#SEL }" ;;
  "SELEL "*) CMD="SELEL:${LINE#SELEL }" ;;
  BACK|RELOAD|ACCEPT|REJECT) CMD="$LINE" ;;
  *) exit 0 ;;
esac
SEQF="$PKG/knowledge.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/knowledge_action.txt"
