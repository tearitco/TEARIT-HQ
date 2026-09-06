#!/bin/sh
# <item action="'irc_item.sh' 'ROOM <name>'"> : $1="ROOM <name>" $2=pkg $3=house
LINE="$1"; PKG="$2"
case "$LINE" in
  "ROOM "*)    CMD="ROOM:${LINE#ROOM }" ;;
  RESCAN)      CMD="RESCAN" ;;
  *) exit 0 ;;
esac
SEQF="$PKG/irc_chat.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$CMD" > "$PKG/irc_chat_action.txt"
