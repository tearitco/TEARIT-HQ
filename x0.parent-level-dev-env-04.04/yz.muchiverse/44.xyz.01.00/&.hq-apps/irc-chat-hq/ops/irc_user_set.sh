#!/bin/sh
# <cli_io id="userfield"> action: $1=pkg $2=house $3=typed user name
PKG="$1"; NAME="$3"
[ -z "$NAME" ] && exit 0
NAME=$(printf '%s' "$NAME" | tr '\r\n\t |' '     ')
SEQF="$PKG/irc_chat.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=USER:%s\n' "$N" "$NAME" > "$PKG/irc_chat_action.txt"
